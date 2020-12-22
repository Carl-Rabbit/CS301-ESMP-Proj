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
#include "my_lcd.h"
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
#define NRF_BUFF_SIZE 32
#define DEFAULT_CHANNEL (u8) 40

#define CONNECT 1
#define DISCONNECT 0
#define MAX_CNT 3

#define STOP (u8) 0
#define WAIT (u8) 1
#define SEND_HB (u8) 2
#define SEND_DA (u8) 3
#define CONF (u8) 4


#define HEAD_LEN 2
#define HEART_BEAT (u8) 65
#define PAYLOAD (u8) 66
#define LAST (u8) 67
#define NOT_LAST (u8) 68

#define HB_TIME 1500
#define HB_TRY_TIME 500
#define DELAY_TIME 10

#define TX_BUF_SIZE 1024

// ui
#define X_PADDING 15
#define Y_PADDING 8
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

u8 rcvDataBuffer[TX_BUF_SIZE];  // uart 接收到的segment组合的buffer。至少和发端能发的长度一致
u8 * rcvDataPtr = rcvDataBuffer;         // 上面个的指针，指示下一个分片的开始

int HBTime = HB_TRY_TIME;
u8 connectCnt = MAX_CNT;


extern MsgType SEND_TYPE;
extern MsgType RESP_TYPE;
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
		if (status == STOP) {
			print("[STM] Please open first.\r\n");
			uartDataLen = 0;
			return;
		}
		strcpy(p_str, ptr2 + 1);    // 拿到<data>，因为空格+1

		// 切割一下包
		int dataLen = uartDataLen - 5;
		uartRestDataCnt = ceil(1.0 * dataLen / NRF_BUFF_SIZE);
		dataPtr = uartDataBuffer;

//		sprintf(txBuffer, "uartRestDataCnt=%d\r\n", uartRestDataCnt);
//		HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

		strcpy(uartDataBuffer, p_str);      // 重新放回去
		uartDataReady = 1;             // 设置发射位
	} else if (strcmp(p_str, "help") == 0) {
		print("[STM] Help\r\nsend <data> - Send data\r\nhelp - Show help\r\nopen - Open connection\r\n"
		"stop - Stop connection\r\nconf <field> <data> - Set the config:\r\n\t\tchannel <INTEGER 40-90>\r\n");
	} else if (strcmp(p_str, "conf") == 0) {
		ptr = ptr2;
		ptr2 = parseNextWord(ptr, &len);
		strncpy(p_str, ptr, len);
		if (strcmp(p_str, "channel") == 0) {
			// set channel
			print("Set channel\r\n");
		}
	} else if (strcmp(p_str, "open") == 0) {
		if (status == STOP) {
			print("[STM] Open");
			nxtStatus = WAIT;
		} else {
			print("[STM] Already open");
		}
	} else if (strcmp(p_str, "stop") == 0) {
		if (status != STOP) {
			print("[STM] Stop");
			nxtStatus = STOP;
		} else {
			print("[STM] Already stop");
		}
	} else {
		sprintf(txBuffer, "Unknown command '%s'. Input 'help' to see all commands.\r\n", p_str);
		HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
	}
	uartDataLen = 0;
}

void SetSndPackage(u8 type, u8 isLast, const u8 * content, u8 length) {
	sndPackage[0] = type;
	sndPackage[1] = isLast;

//	sprintf(txBuffer, ">>> type=%d, isLast=%d. \r\n", type, isLast);
//	HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

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
	static u8 msg[64];
	if (connect == CONNECT) {
		print("[STM] Connected\r\n");
		sprintf(msg, "Channel: %d   Connected   ", channel);
	} else {
		print("[STM] Disconnected\r\n");
		sprintf(msg, "Channel: %d   Disconnected", channel);
	}
	BACK_COLOR = WHITE;
	POINT_COLOR = BLACK;
	LCD_ShowString(X_PADDING,Y_PADDING + 16,lcddev.width-1,32,16, msg);
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

	static u8 runMsg[32];

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

	InitWindow();
//	AddLast((uint8_t *) "Hello World!", RESP_TYPE);
	RefreshWindow();

	POINT_COLOR = BLACK;
	BACK_COLOR = WHITE;
	while (NRF24L01_Check()) {
		LCD_ShowString(X_PADDING, Y_PADDING, 200, 16, 16, "NRF24L01 Error");
		delay_ms(200);
		LCD_Fill(30, 130, 239, 130 + 16, WHITE);
		delay_ms(200);
	}
	LCD_ShowString(X_PADDING, Y_PADDING, 200, 16, 16, "NRF24L01 OK");

//	connect = DISCONNECT;
//	ChangeConnection();

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

	status = STOP;
	nxtStatus = STOP;

	LED0 = 1;
	LED1 = 1;

	print("[STM] Start! Input 'help' to see the command.\r\n");
	while (1) {
		if (nxtStatus == STOP) {
			status = STOP;
			LED1 = 1;

			key = KEY_Scan(0);
			if (key == KEY1_PRES) {
				print("[STM] Open\r\n");
				status = WAIT;
				nxtStatus = WAIT;
				t = 0;
				LED1 = 0;
				connect = DISCONNECT;
				ChangeConnection();
				continue;
			}

			if (t == 500 / DELAY_TIME) {
				POINT_COLOR = BLACK;
				BACK_COLOR = WHITE;
				LCD_ShowString(X_PADDING, Y_PADDING + 16, 230, 16, 16, "Presss KEY1 to open"); //闪烁显示提示信息
			}
			if (t == 1000 / DELAY_TIME) {
				LCD_Fill(X_PADDING, Y_PADDING + 16, 230, Y_PADDING + 32, WHITE);
				t = 0;
			}
			++t;
			delay_ms(DELAY_TIME);
			continue;
		}

		key = KEY_Scan(0);
		if (key == KEY0_PRES) {
			print("[STM] Stop\r\n");
			LED1 = 1;
			nxtStatus = STOP;

			POINT_COLOR = BLACK;
			BACK_COLOR = WHITE;
			sprintf(runMsg, "Stop");
			LCD_Fill(X_PADDING + 110, Y_PADDING, lcddev.width-1,Y_PADDING + 16, WHITE);
			LCD_Fill(X_PADDING, Y_PADDING + 16, lcddev.width-1,Y_PADDING + 16, WHITE);
			LCD_ShowString(X_PADDING + 110, Y_PADDING,lcddev.width-1,32, 16, runMsg);
			continue;
		}

		if (status == WAIT && (nxtStatus == SEND_DA || nxtStatus == SEND_HB)) {
			NRF24L01_TX_Mode_P(channel);
		} else if (status != WAIT && nxtStatus == WAIT) {
			NRF24L01_RX_Mode_P(channel);
		}
		status = nxtStatus;

		u8 isLast = (uartRestDataCnt <= 1) ? LAST: NOT_LAST;

		switch (status) {
			case WAIT:
				// 优先check心跳包
				if(NRF24L01_RxPacket(rcvPackage) == 0) {
					rcvPackage[NRF_BUFF_SIZE] = 0;
					LED0 = 0;
					switch (rcvPackage[0]) {
						case HEART_BEAT:
//							print("Recv HB package\r\n");
							break;
						case PAYLOAD:
							if (rcvPackage[1] == LAST) {
								// 最后一个包
								u8 * tmp = getRcvPayload();
								unsigned int tmp_len = strlen(tmp);
								strncpy(rcvDataPtr, tmp, tmp_len);
								AddLast(rcvDataBuffer, RESP_TYPE);
								RefreshWindow();
								rcvDataPtr = rcvDataBuffer;

								print("[STM] Recv finish: ");
							} else {
								u8 * tmp = getRcvPayload();
								unsigned int tmp_len = strlen(tmp);
								strncpy(rcvDataPtr, tmp, tmp_len);
								rcvDataPtr += tmp_len - 1;

								print("[STM] Recv partly: ");
							}
							print(getRcvPayload());
							print("\r\n");
							break;
						default:
							print("[STM] Unkonwn pacakge\r\n");
					}
					LED0 = 1;
				}
				// XXXXXXXXXXXXXXB123456789012345678XXXXXXXXXXXXXXB123456789012345678
				if (uartRestDataCnt != 0) {
					// 仍有要发的东西
					nxtStatus = SEND_DA;
					break;
				}
				if (uartDataReady == 1) {
					nxtStatus = SEND_DA;
					break;
				}
				if (t >= HBTime / DELAY_TIME) {
					t = 0;
					nxtStatus = SEND_HB;
					break;
				}
				break;

			case SEND_HB:
				LED0 = 0;
				SetSndPackage(HEART_BEAT, LAST, "HB", 3);
				if (NRF24L01_TxPacket(sndPackage) == TX_OK) {
					HBTime = HB_TIME;
					if (connect == DISCONNECT) {
						connect = CONNECT;
						connectCnt = MAX_CNT;
						ChangeConnection();
					}
				} else {
					if (connect == CONNECT) {
						HBTime = HB_TRY_TIME;   // 在CONNECT下失败了，立即加快HB发送
						if (connectCnt <= 1) {
							connect = DISCONNECT;
							ChangeConnection();
						} else {
							-- connectCnt;
						}
					}
				}
				LED0 = 1;
				nxtStatus = WAIT;
				break;

			case SEND_DA:
				SetSndPackage(PAYLOAD, isLast, dataPtr, strlen(dataPtr));
				LED0 = 0;
				u8 ret = NRF24L01_TxPacket(sndPackage);
				if (ret == TX_OK) {
					HBTime = HB_TIME;
					if (connect == DISCONNECT) {
						connect = CONNECT;
						connectCnt = MAX_CNT;
						ChangeConnection();
					}

					if (isLast == LAST) {
						sprintf(txBuffer, "[STM] Send finish: %s\r\n", sndPackage + HEAD_LEN);
						HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

						AddLast(uartDataBuffer, SEND_TYPE);

						uartRestDataCnt = 0;
						uartDataReady = 0;
						uartDataLen = 0;

					} else {
						sprintf(txBuffer, "[STM] Send partly: %s\r\n", sndPackage + HEAD_LEN);
						HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
						-- uartRestDataCnt;
						dataPtr += NRF_BUFF_SIZE - HEAD_LEN;
					}
				} else {
					if (connect == CONNECT) {
						HBTime = HB_TRY_TIME;   // 在CONNECT下失败了，立即加快HB发送
						if (connectCnt < 1) {
							connect = DISCONNECT;
							ChangeConnection();
							// 放弃剩下的
							uartRestDataCnt = 0;
							uartDataReady = 0;
							uartDataLen = 0;
						} else {
							-- connectCnt;
						}
					} else {
						// 放弃剩下的
						uartRestDataCnt = 0;
						uartDataReady = 0;
						uartDataLen = 0;
					}
					sprintf(txBuffer, "[STM] Send failed: %s\r\n", uartDataBuffer);
					HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);

//					sprintf(txBuffer, "[STM] Tx ret: %d\r\n", ret);
//					HAL_UART_Transmit(&huart1, txBuffer, strlen(txBuffer), 0xffff);
				}
				LED0 = 1;
				nxtStatus = WAIT;       // 回到普通模式检测一下
				break;
		}

		if (t % 50 == 0) {
			static int cnt = 0;
			BACK_COLOR = WHITE;
			POINT_COLOR = BLACK;
			if (cnt == 0) {
				sprintf(runMsg, "Runing");
			} else if (cnt == 1) {
				sprintf(runMsg, "Runing.");
			} else if (cnt == 2) {
				sprintf(runMsg, "Runing..");
			} else {
				sprintf(runMsg, "Runing...");
				cnt = -1;
			}
			++ cnt;
			LCD_Fill(X_PADDING + 110, Y_PADDING, lcddev.width-1,Y_PADDING + 16, WHITE);
			LCD_ShowString(X_PADDING + 110, Y_PADDING,lcddev.width-1,32, 16, runMsg);
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