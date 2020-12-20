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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NRF_BUFF_SIZE 32

#define MASTER (u8) 10
#define M_NORM (u8) 11
#define M_WAIT (u8) 12

#define HEART_BEAT_CNT 1000
#define HB_WAIT_CNT 3000


#define SLAVE (u8) 20
#define S_NORM (u8) 21
#define S_REAP (u8) 22


#define HEAD_LEN 1
#define HEART_BEAT (u8) 0
#define HEART_BEAT_REPLY (u8) 1

#define DEFAULT_CHANNEL 40

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
/* USER CODE BEGIN PV */
uint8_t rxBuffer[1024];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {
	u8 key, mode;
	u16 t = 0;
	u8 tmp_buf[NRF_BUFF_SIZE + 1];

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
	LCD_ShowString(30, 50, 200, 16, 16, "Mini STM32 1612");
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


	while (1) {
		key = KEY_Scan(0);
		if (key == KEY0_PRES) {
			mode = SLAVE;
			break;
		} else if (key == KEY1_PRES) {
			mode = MASTER;
			break;
		}
		t++;
		if (t == 100)LCD_ShowString(10, 150, 230, 16, 16, "KEY1:MASTER KEY0:SLAVE"); //闪烁显示提示信息
		if (t == 200) {
			LCD_Fill(10, 150, 230, 150 + 16, WHITE);
			t = 0;
		}
		delay_ms(5);
	}

	if (mode == MASTER) {
		mode = M_NORM;
		LCD_ShowString(30, 150, 200, 16, 16, "NRF24L01 MASTER");
		NRF24L01_TX_Mode_P(DEFAULT_CHANNEL);
		while (1) {
			switch (mode) {
				case M_NORM:
					if (t == HEART_BEAT_CNT) {

						// 发送心跳包
						LED1 = 1;
						t = 0;
						tmp_buf[0] = HEART_BEAT;
						strcpy(tmp_buf + HEAD_LEN, "This is a data");
						tmp_buf[NRF_BUFF_SIZE] = 0;     // 加入结束符

						if (NRF24L01_TxPacket(tmp_buf) == TX_OK) {
							// 发送成功
							LCD_ShowString(30, 170, 239, 32, 16, "Sended DATA:");
							LCD_ShowString(0, 190, lcddev.width - 1, 32, 16, tmp_buf + HEAD_LEN);

							mode = M_WAIT;
						} else {
							// 发送失败
							LCD_Fill(0, 170, lcddev.width, 170 + 16 * 3, WHITE);//清空显示
							LCD_ShowString(30, 170, lcddev.width - 1, 32, 16, "Send Failed ");
						}
					}
					break;
				case M_WAIT:
					if (t == HB_WAIT_CNT) {
						// 等待回复超时
						t = 0;
						mode = M_NORM;
					}
					break;
			}
			t++;
			delay_ms(1);
		}
	} else {
		mode = S_NORM;
		LCD_ShowString(30, 150, 200, 16, 16, "NRF24L01 SLAVE");
		LCD_ShowString(30, 170, 200, 16, 16, "Received DATA:");
		NRF24L01_RX_Mode_P(DEFAULT_CHANNEL);
		strcpy(tmp_buf, "This is the data send by Master");
		while (1) {
			switch (mode) {
				case S_NORM:
					if (NRF24L01_RxPacket(tmp_buf) == 0) {
						// 接收到了一个包
						tmp_buf[NRF_BUFF_SIZE] = 0;//加入字符串结束符
						switch (tmp_buf[0]) {
							case HEART_BEAT:
								// 是心跳包，准备回发
								LCD_ShowString(0, 190, lcddev.width - 1, 32, 16, tmp_buf + HEAD_LEN);
								mode = S_REAP;
								break;
							case HEART_BEAT_REPLY:

								break;
							default:
								// error
								break;
						}
					} else {
						delay_us(100);
					}
					t++;
					if (t == 10000)//大约1s钟改变一次状态
					{
						t = 0;
						LED1 = !LED1;
					}
					break;
				case S_REAP:

					break;
			}
		}
	}
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		static unsigned char uRx_Data[1024] = "hello ";
		static unsigned char uLength = 7;
		if (rxBuffer[0] == '\n') {
			uRx_Data[uLength++] = '\n';
			uRx_Data[uLength++] = '\0';
			HAL_UART_Transmit(&huart1, uRx_Data, uLength, 0xffff);
			uLength = 7;
		} else {
			uRx_Data[uLength++] = rxBuffer[0];
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