# Sistema de Cálculo de Pose

Este repositório contém a implementação de um sistema de estimação de pose, desenvolvido em plataforma STM32 NUCLEO-F767ZI. O sistema utiliza um sensor MPU6050 para o cálculo das acelerações e velocidades angulares, além de um display SSD1306 para visualização dos valores desejados. O fundamento dessa tarefa é a utilização do protocolo de comunicação I2C entre os sensores e o microcontrolador.

## Fundamentos

O sensor MPU6050 é conhecido por sua ampla utilização em drones, a fim de fazer a estimação de pose. Uma implementação comum deste sensor neste contexto é o cálculo dos ângulos que um drone/aeronave pode fazer em torno dos três eixos cartesianos: Pitch, Roll e Yaw.


