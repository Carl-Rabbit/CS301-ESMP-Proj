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
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NRF_BUFF_SIZE 64
#define DEFAULT_CHANNEL (u8) 40

#define CONNECT 1
#define DISCONNECT 0

#define WAIT (u8) 0
#define SEND_HB (u8) 1
#define SEND_DA (u8) 2
#define CONF (u8) 3


#define HEAD_LEN 2
#define HEART_BEAT (u8) 65
#define PAYLOAD (u8) 66
#define FIRST (u8) 65
#define NOT_FIRST (u8) 66
#define LAST (u8) 67
#define NOT_LAST (u8) 68

#define HB_TIME 1000
#define DELAY_TIME 10

#define TX_BUF_SIZE 8
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
u8 uartDataLen = 0;                     // 串口输入数据的长度
u8 uartDataReady = 0;                   // 待发送的mark：0不发送，1可以发送

u8 uartRestDataCnt = 0;     // 还剩几个包
u8 * dataPtr = NULL;        // 下一个包的payload开始的地方

u8 p_str[TX_BUF_SIZE];       // 用于临时处理的str，至少和uart的buffer一样长

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

u8 * parseNextWord(u8 * str, int * lenPtr) {
	int len = 0;
	u8 * ptr = str;
	while (1) {
		u8 chr = *ptr;
		if (chr == (u8) '\r' || chr == (u8) '\n'
				|| chr == (u8) '\0' || chr == (u8) ' ') {
			*lenPtr = len;
			return ptr;
		}
		++ len;
		++ ptr;
	}
}

/* 当串口输入完成后调用，uartDataBuffer是输入的数据（长度uartDataLen) */
void Processing_UART() {
	if (uartDataLen <= 1) {
		// 空输入
		uartDataLen = 0;
		return;
	}
	/**
	 * send <data>: 发送数据
	 * help: 打印help
	 * conf <field> <data>: 修改数据
	 */
	int len = 0;
	u8 * ptr = uartDataBuffer;
	u8 * ptr2 = parseNextWord(ptr, &len);
	strncpy(p_str, ptr, len);
	p_str[len] = '\0';

	if (strcmp(p_str, "send") == 0) {
		strcpy(p_str, ptr2 + 1);    // 拿到<data>，因为空格+1

		// 切割一下包
		int dataLen = uartDataLen - 5;
		uartRestDataCnt = ceil(1.0 * dataLen / NRF_BUFF_SIZE);
		dataPtr = uartDataBuffer;

		sprintf(txBuffer, "uartRestDataCnt=%d\r\n", uartRestDataCnt);
		HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

		strcpy(uartDataBuffer, p_str);      // 重新放回去
		uartDataReady = 1;             // 设置发射位
	} else if (strcmp(p_str, "help") == 0) {
		print("[STM] Help\r\n");
	} else if (strcmp(p_str, "conf") == 0) {
		ptr = ptr2;
		ptr2 = parseNextWord(ptr, &len);
		strncpy(p_str, ptr, len);
		if (strcmp(p_str, "channel") == 0) {
			// set channel
			print("Set channel\r\n");
		}
	} else {
		sprintf(txBuffer, "Unknown command '%s'. Input 'help' to see all commands.\r\n", p_str);
		HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
	}
	uartDataLen = 0;
}

void SetSndPackage(u8 type, u8 isLast, const u8 * content, u8 length) {
	sndPackage[0] = type;
	sndPackage[2] = isLast;

	sprintf(txBuffer, ">>> type=%d, isLast=%d. \r\n", type, isLast);
	HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

	int i;
	for (i = HEAD_LEN; i < length + HEAD_LEN && i < NRF_BUFF_SIZE; ++i) {
		sndPackage[i] = content[i - HEAD_LEN];

//		sprintf(txBuffer, ">>> i=%d, chr=%c \r\n", i, sndPackage[i]);
//		HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
	}
	sndPackage[i] = '\0';
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
	LCD_ShowString(30, 50, 200, 16, 16, "Mini STM32 2135");
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

		u8 isLast = (uartRestDataCnt == 0) ? LAST: NOT_LAST;

		switch (status) {
			case WAIT:
				if(NRF24L01_RxPacket(rcvPackage) == 0) {
					rcvPackage[NRF_BUFF_SIZE] = 0;
					switch (rcvPackage[0]) {
						case HEART_BEAT:
//							print("Recv HB package\r\n");
							break;
						case PAYLOAD:
							print("[STM] Recv Payload: ");
							print(getRcvPayload());
							print("\r\n");
							break;
						default:
							print("[STM] Unkonwn pacakge\r\n");
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
				SetSndPackage(HEART_BEAT, LAST, "HB", 3);
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
				SetSndPackage(PAYLOAD, isLast, dataPtr, strlen(dataPtr));
				if (NRF24L01_TxPacket(sndPackage) == TX_OK) {
					if (connect == DISCONNECT) {
						connect = CONNECT;
						ChangeConnection();
					}
					if (isLast == LAST) {
						sprintf(txBuffer, "[STM] Send successfully: %s", uartDataBuffer);
						HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
						uartDataReady = 0;
						uartDataLen = 0;
						nxtStatus = WAIT;
					} else {
						print("send partly");
						-- uartRestDataCnt;
						dataPtr += NRF_BUFF_SIZE;
					}
				} else {
					if (connect == CONNECT) {
						connect = DISCONNECT;
						ChangeConnection();
					}
					sprintf(txBuffer, "[STM] Send failed: %s", uartDataBuffer);
					HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

					// 放弃剩下的
					uartDataReady = 0;
					uartDataLen = 0;
					nxtStatus = WAIT;
				}
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

			sprintf(txBuffer, "uartDataBuffer=\"%s\"\r\n", uartDataBuffer);
			HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

			Processing_UART();
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