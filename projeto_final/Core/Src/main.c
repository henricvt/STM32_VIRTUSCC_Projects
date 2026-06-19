/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#define ARM_MATH_CM4
#include "arm_math.h"
#include "ssd1306.h"
#include "MPU6050.h"
#include "math.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    FAULT_OK = 0,
    FAULT_UNBALANCE,
    FAULT_LOOSENESS,
    FAULT_LUBRICATION,
    FAULT_UNKNOWN

} FaultType_t;

typedef struct
{
    float rms;

    float peak1_freq;
    float peak1_amp;

    float peak2_freq;
    float peak2_amp;

    float peak3_freq;
    float peak3_amp;

    FaultType_t fault;

} Diagnostic_t;

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    Diagnostic_t diag;

} LogEntry_t;

typedef struct
{
    float avg_rms;

    float avg_peak1_amp;
    float avg_peak2_amp;
    float avg_peak3_amp;

    uint32_t samples_used;

} HistoryStats_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SAMPLE_RATE_HZ 1000.0f
#define FFT_SIZE 2048
#define LOG_SIZE_FLASH (((sizeof(LogEntry_t) + 7) & ~7))

#define ALARM_MULTIPLIER 2.0f
#define ALARM_RESET_MULTIPLIER 1.6f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

float samples[FFT_SIZE];

float fft_buffer[FFT_SIZE];

float fft_mag[FFT_SIZE/2];

arm_rfft_fast_instance_f32 fft;

Diagnostic_t diag;

float shaft_freq_hz = 22.5f;

LogEntry_t current_log;

HistoryStats_t history;

uint32_t flash_write_address = 0x08060000;

float Ax, Ay, Az;

float Az_buffer[SAMPLE_SIZE];

volatile uint16_t sample_index = 0;
volatile uint8_t sample_flag = 0;
volatile uint8_t buffer_ready = 0;

float Az_rms_calibrated = 0.0f;
float Az_alarm_level = 0.0f;
float Az_reset_level = 0.0f;

uint8_t alarm_active = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

void RTC_SetNextAlarm8h(void);
void EnterStandby(void);
float Calculate_RMS(float *samples);

void FFT_Process(float *input);
void FFT_FindPeaks(void);
void DiagnoseFault(void);

void PrepareLogEntry(void);
void SaveToFlash(LogEntry_t *entry);
uint32_t FindNextFlashAddress(void);
void CalculateHistoryStats(void);
uint8_t IsWithinLast30Days(LogEntry_t *log);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Remove_DC(float *buffer, uint16_t size)
{
    float mean = 0.0f;

    for (uint16_t i = 0; i < size; i++)
    {
        mean += buffer[i];
    }

    mean /= size;

    for (uint16_t i = 0; i < size; i++)
    {
        buffer[i] -= mean;
    }
}

float Calculate_RMS(float *buffer, uint16_t size)
{
    float sum = 0.0f;

    for (uint16_t i = 0; i < size; i++)
    {
        sum += buffer[i] * buffer[i];
    }

    return sqrtf(sum / size);
}

void Collect_Z_Block(void)
{
    sample_index = 0;
    buffer_ready = 0;

    while (!buffer_ready)
    {
        if (sample_flag)
        {
            sample_flag = 0;

            MPU6050_Read_Accel(&Ax, &Ay, &Az);

            Az_buffer[sample_index] = Az;

            sample_index++;

            if (sample_index >= SAMPLE_SIZE)
            {
                sample_index = 0;
                buffer_ready = 1;
            }
        }
    }
}

void Show_Calibrating_Message(void)
{
    SSD1306_Clear();
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts("Calibrando...", &Font_7x10, 1);
    SSD1306_GotoXY(0, 16);
    SSD1306_Puts("Aguarde", &Font_7x10, 1);
    SSD1306_UpdateScreen();
}

void Show_Calibration_Done_Message(void)
{
    SSD1306_Clear();
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts("Calibracao", &Font_7x10, 1);
    SSD1306_GotoXY(0, 16);
    SSD1306_Puts("concluida", &Font_7x10, 1);
    SSD1306_UpdateScreen();
}

void Calibrate_Normal_Vibration(void)
{
    Show_Calibrating_Message();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

    Collect_Z_Block();

    Remove_DC(Az_buffer, SAMPLE_SIZE);

    Az_rms_calibrated = Calculate_RMS(Az_buffer, SAMPLE_SIZE);

    Az_alarm_level = ALARM_MULTIPLIER * Az_rms_calibrated;
    Az_reset_level = ALARM_RESET_MULTIPLIER * Az_rms_calibrated;

    Show_Calibration_Done_Message();

    HAL_Delay(1000);
}

void UART_Send_String(char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void Send_Z_Buffer_CSV(float *buffer, uint16_t size)
{
    char msg[64];

    UART_Send_String("BEGIN_AZ\r\n");

    sprintf(msg, sizeof(msg), "FS,%.1f\r\n", FS);
    UART_Send_String(msg);

    snprintf(msg, sizeof(msg), "N,%u\r\n", size);
    UART_Send_String(msg);

    UART_Send_String("index,Az\r\n");

    for (uint16_t i = 0; i < size; i++)
    {
        sprintf(msg, sizeof(msg), "%u,%.6f\r\n", i, buffer[i]);
        UART_Send_String(msg);
    }

    UART_Send_String("END_AZ\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  arm_rfft_fast_init_f32(
      &fft,
      FFT_SIZE);

  if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x1234)
  {
      CalibrationRoutine(); //calibração

      HAL_RTCEx_BKUPWrite(
          &hrtc,
          RTC_BKP_DR0,
          0x1234);
  }

  if(ButtonPressed())
  {
      HAL_RTCEx_BKUPWrite(
          &hrtc,
          RTC_BKP_DR0,
          0);

      NVIC_SystemReset();
  }

  flash_write_address = FindNextFlashAddress();

  MPU6050_init();
  SSD1306_Init();

  SSD1306_Clear();
  SSD1306_UpdateScreen();

  HAL_TIM_Base_Start_IT(&htim2);

  CollectFFTBlock();

  HAL_TIM_Base_Stop_IT(&htim2);

  Remove_DC(samples, FFT_SIZE);

  diag.rms = Calculate_RMS(samples, FFT_SIZE);

  FFT_Process(samples);

  FFT_FindPeaks();

  CalculateHistoryStats();

  DiagnoseFault();

  PrepareLogEntry();

  SaveToFlash(&current_log);


  if(diag.fault == FAULT_OK)
  {
      RTC_SetNextAlarm8h();

      EnterStandby();
  }
  else
  {
      OLED_ShowDiagnostic(&diag);

      while(1)
      {
          HAL_GPIO_TogglePin(
              LED_GPIO_Port,
              LED_Pin);

          Buzzer_Beep();

          HAL_Delay(1000);
      }
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x26;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm A
  */
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x0;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 8999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(USART_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        sample_flag = 1;
    }
}

void RTC_SetNextAlarm8h(void)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    RTC_AlarmTypeDef alarm = {0};

    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    uint32_t total_seconds =
        (time.Hours * 3600UL) +
        (time.Minutes * 60UL) +
        time.Seconds;

    total_seconds += (8UL * 3600UL);

    total_seconds %= 86400UL;

    alarm.AlarmTime.Hours =
        total_seconds / 3600UL;

    alarm.AlarmTime.Minutes =
        (total_seconds % 3600UL) / 60UL;

    alarm.AlarmTime.Seconds =
        total_seconds % 60UL;

    alarm.AlarmTime.SubSeconds = 0;

    alarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;

    alarm.AlarmSubSecondMask =
        RTC_ALARMSUBSECONDMASK_ALL;

    alarm.AlarmTime.DayLightSaving =
        RTC_DAYLIGHTSAVING_NONE;

    alarm.AlarmTime.StoreOperation =
        RTC_STOREOPERATION_RESET;

    alarm.Alarm = RTC_ALARM_A;

    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);

    HAL_RTC_SetAlarm_IT(
        &hrtc,
        &alarm,
        RTC_FORMAT_BIN);
}

void EnterStandby(void)
{
    HAL_TIM_Base_Stop(&htim2);

    HAL_I2C_DeInit(&hi2c1);

    HAL_UART_DeInit(&huart2);

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    HAL_PWR_EnterSTANDBYMode();
}

void FFT_Process(float *input)
{
    arm_rfft_fast_f32(
        &fft,
        input,
        fft_buffer,
        0);

    arm_cmplx_mag_f32(
        fft_buffer,
        fft_mag,
        FFT_SIZE/2);
}

void FFT_FindPeaks(void)
{
    uint32_t peak1 = 0;
    uint32_t peak2 = 0;
    uint32_t peak3 = 0;

    for(uint32_t i = 2; i < FFT_SIZE/2; i++)
    {
        if(fft_mag[i] > fft_mag[peak1])
        {
            peak3 = peak2;
            peak2 = peak1;
            peak1 = i;
        }
        else if(fft_mag[i] > fft_mag[peak2])
        {
            peak3 = peak2;
            peak2 = i;
        }
        else if(fft_mag[i] > fft_mag[peak3])
        {
            peak3 = i;
        }
    }

    const float df = SAMPLE_RATE_HZ / FFT_SIZE;

    diag.peak1_freq = peak1 * df;
    diag.peak1_amp  = fft_mag[peak1];

    diag.peak2_freq = peak2 * df;
    diag.peak2_amp  = fft_mag[peak2];

    diag.peak3_freq = peak3 * df;
    diag.peak3_amp  = fft_mag[peak3];
}

void PrepareLogEntry(void)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    HAL_RTC_GetTime(&hrtc,&time,RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc,&date,RTC_FORMAT_BIN);

    current_log.year  = 2000 + date.Year;
    current_log.month = date.Month;
    current_log.day   = date.Date;

    current_log.hour   = time.Hours;
    current_log.minute = time.Minutes;
    current_log.second = time.Seconds;

    current_log.diag = diag;
}

void SaveToFlash(LogEntry_t *entry)
{
    HAL_FLASH_Unlock();

    uint64_t *data = (uint64_t *)entry;

    uint32_t size = LOG_SIZE_FLASH / 8;

    if(flash_write_address + LOG_SIZE_FLASH >= 0x08080000)
    {
        FLASH_EraseInitTypeDef erase;
        uint32_t error;

        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Sector = FLASH_SECTOR_7;
        erase.NbSectors = 1;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        HAL_FLASHEx_Erase(
            &erase,
            &error);

        flash_write_address = 0x08060000;
    }

    for(uint32_t i=0;i<size;i++)
    {
        HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_DOUBLEWORD,
            flash_write_address,
            data[i]);

        flash_write_address += 8;
    }

    HAL_FLASH_Lock();
}

uint32_t FindNextFlashAddress(void)
{
    uint32_t addr = 0x08060000;

    while(addr < 0x08080000)
    {
        uint64_t first_word =
            *(uint64_t*)addr;

        if(first_word == 0xFFFFFFFFFFFFFFFFULL)
            return addr;

        addr += LOG_SIZE_FLASH;
    }

    return 0x08060000;
}

void DiagnoseFault(void)
{
    diag.fault = FAULT_OK;

    if(history.samples_used < 5)
        return;

    if(diag.rms >
       history.avg_rms * 1.5f)
    {
        diag.fault =
            FAULT_UNBALANCE;
    }

    if(diag.peak1_amp >
       history.avg_peak1_amp * 1.5f)
    {
        diag.fault =
            FAULT_UNBALANCE;
    }

    if(diag.peak2_amp >
       history.avg_peak2_amp * 2.0f)
    {
        diag.fault =
            FAULT_LOOSENESS;
    }

    if(diag.peak3_amp >
       history.avg_peak3_amp * 2.0f)
    {
        diag.fault =
            FAULT_LUBRICATION;
    }
}

uint8_t IsWithinLast30Days(LogEntry_t *log)
{
    RTC_TimeTypeDef now_time;
    RTC_DateTypeDef now_date;

    HAL_RTC_GetTime(
        &hrtc,
        &now_time,
        RTC_FORMAT_BIN);

    HAL_RTC_GetDate(
        &hrtc,
        &now_date,
        RTC_FORMAT_BIN);

    uint32_t current_days =
        (2000 + now_date.Year) * 365 +
        now_date.Month * 30 +
        now_date.Date;

    uint32_t log_days =
        log->year * 365 +
        log->month * 30 +
        log->day;

    if(current_days >= log_days)
    {
        return ((current_days - log_days) <= 30);
    }

    return 0;
}

void CalculateHistoryStats(void)
{
    memset(
        &history,
        0,
        sizeof(history));

    uint32_t addr = 0x08060000;

    while(addr < flash_write_address)
    {
        LogEntry_t *log =
            (LogEntry_t*)addr;

        if(IsWithinLast30Days(log))
        {
            history.avg_rms +=
                log->diag.rms;

            history.avg_peak1_amp +=
                log->diag.peak1_amp;

            history.avg_peak2_amp +=
                log->diag.peak2_amp;

            history.avg_peak3_amp +=
                log->diag.peak3_amp;

            history.samples_used++;
        }

        addr += LOG_SIZE_FLASH;
    }

    if(history.samples_used == 0)
        return;

    history.avg_rms /=
        history.samples_used;

    history.avg_peak1_amp /=
        history.samples_used;

    history.avg_peak2_amp /=
        history.samples_used;

    history.avg_peak3_amp /=
        history.samples_used;
}

void CollectFFTBlock(void)
{
    for(uint32_t i = 0; i < FFT_SIZE; i++)
    {
        sample_flag = 0;

        while(!sample_flag);

        MPU6050_Read_Accel(&Ax, &Ay, &Az);

        samples[i] = Az;
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
