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
#include "ssd1306.h"
#include "MPU6050.h"
#include "math.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c4;

/* USER CODE BEGIN PV */

float Ax, Ay, Az;
float Gx, Gy, Gz;

float accel_pitch, accel_roll;
float pitch, roll;

uint32_t ultimo_tempo_botao = 0;
uint32_t tempo_display = 0;
uint8_t tela_anterior = 3;

#define PI 3.14159265358979323846

volatile uint8_t control = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_I2C1_Init();
  MX_I2C4_Init();
  /* USER CODE BEGIN 2 */

  SSD1306_Init();
  MPU6050_init();

  MPU6050_Read_Accel(&Ax, &Ay, &Az);
  pitch = atan2(Ax, sqrt(Ay * Ay + Az * Az)) * (180.0 / PI);
  roll  = atan2(Ay, sqrt(Ax * Ax + Az * Az)) * (180.0 / PI);

  uint32_t tempo_anterior = HAL_GetTick();

  SSD1306_Clear();

  SSD1306_GotoXY(40, 0);
  SSD1306_Puts("Angulos", &Font_7x10, 1);

  SSD1306_GotoXY(15, 16);
  SSD1306_Puts("Pitch:", &Font_7x10, 1);
  SSD1306_GotoXY(15, 30);
  SSD1306_Puts("Roll: ", &Font_7x10, 1);
  SSD1306_GotoXY(15, 44);
  SSD1306_Puts("Yaw:  ", &Font_7x10, 1);

  SSD1306_UpdateScreen();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

	  MPU6050_Read_Accel(&Ax,&Ay,&Az);
	  MPU6050_Read_Gyro(&Gx, &Gy, &Gz);

	  uint32_t tempo_atual = HAL_GetTick();
	  float dt = (tempo_atual - tempo_anterior) / 1000.0;
	  tempo_anterior = tempo_atual;

	  char buffer_float[16];

	  if(dt > 0){

		  accel_pitch = atan2(Ax, sqrt(Ay * Ay + Az * Az)) * (180.0/ PI);
		  accel_roll = atan2(Ay, sqrt(Ax * Ax + Az * Az)) * (180.0/ PI);

		  float gx_ang = Gx / 131.0;
		  float gy_ang = Gy / 131.0;

		  pitch = 0.05 * (pitch + gx_ang * dt) + 0.95 * accel_pitch;
		  roll = 0.05 * (roll + gy_ang * dt) + 0.95 * accel_roll;

		  if(tempo_atual - tempo_display >= 100){

			  tempo_display = HAL_GetTick();

			  if(control != tela_anterior){

				  SSD1306_Clear();

				  if(control == 0){

					  SSD1306_GotoXY(40, 0);
					  SSD1306_Puts("Degrees", &Font_7x10, 1);

					  SSD1306_GotoXY(15, 16);
					  SSD1306_Puts("Pitch:", &Font_7x10, 1);
					  SSD1306_GotoXY(15, 30);
					  SSD1306_Puts("Roll: ", &Font_7x10, 1);
					  SSD1306_GotoXY(15, 44);
					  SSD1306_Puts("Yaw: ", &Font_7x10, 1);

				  }

				 if(control == 1){

					 SSD1306_GotoXY(20, 0);
					 SSD1306_Puts("Accelerations", &Font_7x10, 1);

					 SSD1306_GotoXY(15, 16);
					 SSD1306_Puts("Ac.X:", &Font_7x10, 1);
					 SSD1306_GotoXY(15, 30);
					 SSD1306_Puts("Ac.Y: ", &Font_7x10, 1);
					 SSD1306_GotoXY(15, 44);
					 SSD1306_Puts("Ac.Z:", &Font_7x10, 1);

				 }

				 if(control == 2){

					 SSD1306_GotoXY(30, 0);
					 SSD1306_Puts("Velocities", &Font_7x10, 1);

					 SSD1306_GotoXY(15, 16);
					 SSD1306_Puts("Vel.X:", &Font_7x10, 1);
					 SSD1306_GotoXY(15, 30);
					 SSD1306_Puts("Vel.Y: ", &Font_7x10, 1);
					 SSD1306_GotoXY(15, 44);
					 SSD1306_Puts("Vel.Z:", &Font_7x10, 1);

				 }

				 tela_anterior = control;
			  }


			  if(control == 0){

				  SSD1306_GotoXY (65,16);
				  sprintf (buffer_float, "%.3f", pitch);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,30);
				  sprintf (buffer_float, "%.3f", roll);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,44);
				  sprintf (buffer_float, "%.3f", 0.0);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);

			  }

			  if(control == 1){

				  SSD1306_GotoXY (65,16);
				  sprintf (buffer_float, "%.3f", Ax);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,30);
				  sprintf (buffer_float, "%.3f", Ay);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,44);
				  sprintf (buffer_float, "%.3f", Az);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);

			  }


			  if(control == 2){

				  SSD1306_GotoXY (65,16);
				  sprintf (buffer_float, "%.3f", Gx);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,30);
				  sprintf (buffer_float, "%.3f", Gy);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);
				  SSD1306_GotoXY (65,44);
				  sprintf (buffer_float, "%.3f", Gz);
				  SSD1306_Puts (buffer_float, &Font_7x10, 1);

			  }

			  SSD1306_UpdateScreen();

		  }

	  }


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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  hi2c1.Init.Timing = 0x0010061A;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C4_Init(void)
{

  /* USER CODE BEGIN I2C4_Init 0 */

  /* USER CODE END I2C4_Init 0 */

  /* USER CODE BEGIN I2C4_Init 1 */

  /* USER CODE END I2C4_Init 1 */
  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x00303D5B;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C4_Init 2 */

  /* USER CODE END I2C4_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : USER_BUTTON_Pin */
  GPIO_InitStruct.Pin = USER_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == USER_BUTTON_Pin){

			uint32_t tempo_atual_botao = HAL_GetTick();

			if((tempo_atual_botao - ultimo_tempo_botao) > 200){

				control++;

				if(control > 2){
					control = 0;
				}
				ultimo_tempo_botao = tempo_atual_botao;
			}

	}

}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
