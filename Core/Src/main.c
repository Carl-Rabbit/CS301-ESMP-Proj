/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lcd.h"
#include "24l01.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NRF_BUFF_SIZE 32
#define DEFAULT_CHANNEL (u8) 40

#define CONNECT 1
#define DISCONNECT 0

#define WAIT (u8) 0
#define SEND_HB (u8) 1
#define SEND_DA (u8) 2



#define HEAD_LEN 1
#define HEART_BEAT (u8) 65
#define PAYLOAD (u8) 66

#define HB_TIME 1000
#define DELAY_TIME 10

#define TX_BUF_SIZE 64
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

/* USER CODE BEGIN PV */

uint8_t rxBuffer[1024];
u8 key;
u8 status, nxtStatus;
u16 t = 0;
u8 sndPackage[NRF_BUFF_SIZE + 1];
u8 rcvPackage[NRF_BUFF_SIZE + 1];
u8 channel = DEFAULT_CHANNEL;
u8 connect = DISCONNECT;

u8 txBuffer[TX_BUF_SIZE];       // 串口回传的数据
u8 uartDataBuffer[TX_BUF_SIZE];         // 串口输入数据的缓存
u8 uartDataLen = 0;
u8 uartDataReady = 0;                   // 待处理mark
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void print(const u8 * msg) {
	sprintf(txBuffer, msg);
	HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
}

//u8 Processing_UART() {
//
//}

void SetSndPackage(u8 type, const u8 * content, u8 length) {
	sndPackage[0] = type;
	int i;
	for (i = HEAD_LEN; i < length + HEAD_LEN && i < NRF_BUFF_SIZE; ++i) {
		sndPackage[i] = content[i - HEAD_LEN];
	}
	sndPackage[i] = 0;
	sndPackage[NRF_BUFF_SIZE] = 0;
}

void ChangeConnection() {
	sprintf(txBuffer, "Connect=%d\r\n", connect);
	HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
	if (connect == CONNECT) {
		LCD_ShowString(30,170,lcddev.width-1,32,16,"Send HB ");
	} else {
		LCD_ShowString(30,170,lcddev.width-1,32,16,"Send Failed ");
	}
}

u8 * getRcvPayload() {
	return rcvPackage + HEAD_LEN;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

	HAL_Init();                            //初始化HAL库
	Stm32_Clock_Init(RCC_PLL_MUL9);    //设置时钟,72M
	delay_init(72);                    //初始化延时函数
	MX_USART1_UART_Init();
	HAL_UART_Receive_IT(&huart1, (uint8_t *) rxBuffer, 1);
//		uart_init(115200);					//初始化串口
	LED_Init();                            //初始化LED
	KEY_Init();                            //初始化按键
	LCD_Init();                            //初始化LCD
	NRF24L01_Init();                    //初始化NRF24L01

	POINT_COLOR = RED;
	LCD_ShowString(30, 50, 200, 16, 16, "Mini STM32 1909");
	LCD_ShowString(30, 70, 200, 16, 16, "NRF24L01 TEST");
	LCD_ShowString(30, 90, 200, 16, 16, "ATOM@ALIENTEK");
	LCD_ShowString(30, 110, 200, 16, 16, "2019/11/15");


	while (NRF24L01_Check()) {
		LCD_ShowString(30, 130, 200, 16, 16, "NRF24L01 Error");
		delay_ms(200);
		LCD_Fill(30, 130, 239, 130 + 16, WHITE);
		delay_ms(200);
	}
	LCD_ShowString(30, 130, 200, 16, 16, "NRF24L01 OK");


//	while (1) {
//		key = KEY_Scan(0);
//		if (key == KEY0_PRES) {
//			break;
//		}
//		t++;
//		if (t == 100) {
//			LCD_ShowString(10, 150, 230, 16, 16, "Presss KEY0 for default"); //闪烁显示提示信息
//			LCD_ShowString(10, 166, 230, 16, 16, "or input channel"); //闪烁显示提示信息
//		}
//		if (t == 200) {
//			LCD_Fill(10, 150, 230, 150 + 32, WHITE);
//			t = 0;
//		}
//		delay_ms(5);
//	}

	status = WAIT;
	nxtStatus = WAIT;

	while (1) {
		if (status == WAIT && (nxtStatus == SEND_DA || nxtStatus == SEND_HB)) {
			NRF24L01_TX_Mode();
//			HAL_UART_Transmit(&huart1, "Go SEND_XX\r\n", 12, 0xffff);
		} else if ((status == SEND_DA || status == SEND_HB) && nxtStatus == WAIT) {
			NRF24L01_RX_Mode();
//			HAL_UART_Transmit(&huart1, "Go WAIT\r\n", 9, 0xffff);
		}
		status = nxtStatus;
		switch (status) {
			case WAIT:
				if(NRF24L01_RxPacket(rcvPackage) == 0) {
					rcvPackage[NRF_BUFF_SIZE] = 0;
					switch (rcvPackage[0]) {
						case HEART_BEAT:
//							print("Recv HB package\r\n");
							break;
						case PAYLOAD:
							print("Recv Payload: ");
							print(getRcvPayload());
							break;
						default:
							print("Unkonwn pacakge\r\n");
					}
				}

				if (uartDataReady == 1) {
					nxtStatus = SEND_DA;
					break;
				}
				if (t >= HB_TIME / DELAY_TIME) {
					t = 0;
					nxtStatus = SEND_HB;
					break;
				}
				break;

			case SEND_HB:
				SetSndPackage(HEART_BEAT, "Heart beat package", 19);
				if (NRF24L01_TxPacket(sndPackage) == TX_OK) {
					if (connect == DISCONNECT) {
						connect = CONNECT;
						ChangeConnection();
					}
				} else {
					if (connect == CONNECT) {
						connect = DISCONNECT;
						ChangeConnection();
					}
				}
				nxtStatus = WAIT;
				break;

			case SEND_DA:
				SetSndPackage(PAYLOAD, uartDataBuffer, strlen(uartDataBuffer));
				if (NRF24L01_TxPacket(sndPackage) == TX_OK) {
					if (connect == DISCONNECT) {
						connect = CONNECT;
						ChangeConnection();
					}
					sprintf(txBuffer, "Send successfully: %s", uartDataBuffer);
					HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
				} else {
					if (connect == CONNECT) {
						connect = DISCONNECT;
						ChangeConnection();
					}
					sprintf(txBuffer, "Send failed: %s", uartDataBuffer);
					HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
				}
				uartDataReady = 0;
				uartDataLen = 0;
				nxtStatus = WAIT;
				break;
		}
		++t;
		delay_ms(DELAY_TIME);
	}
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		if (rxBuffer[0] == '\n') {
			uartDataBuffer[uartDataLen++] = '\n';
			uartDataBuffer[uartDataLen++] = '\0';
			uartDataReady = 1;
		} else {
			uartDataBuffer[uartDataLen++] = rxBuffer[0];
		}
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

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
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

#pragma clang diagnostic pop