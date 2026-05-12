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
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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

/* USER CODE BEGIN PV */
uint32_t tx_count = 0;
uint32_t rx_count = 0;
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
  MX_USART2_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */

  /* --- 단계별 반환값 저장 --- */
  HAL_StatusTypeDef filter_status;
  HAL_StatusTypeDef start_status;

  /* --- CAN 필터 --- */
  CAN_FilterTypeDef filter = {0};
  filter.FilterBank          = 0;
  filter.FilterMode          = CAN_FILTERMODE_IDMASK;
  filter.FilterScale         = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh        = 0x0000;
  filter.FilterIdLow         = 0x0000;
  filter.FilterMaskIdHigh    = 0x0000;
  filter.FilterMaskIdLow     = 0x0000;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation    = ENABLE;
  filter.SlaveStartFilterBank = 14;
  filter_status = HAL_CAN_ConfigFilter(&hcan1, &filter);

  /* --- CAN Start --- */
  /* Start 전 상태 확인 */
  char filter_msg[80];
  int filter_len = snprintf(filter_msg, sizeof(filter_msg),
      "[FILTER] status=%d\r\n", filter_status);
  HAL_UART_Transmit(&huart2, (uint8_t*)filter_msg, filter_len, 100);

  char pre_msg[80];
  int pre_len = snprintf(pre_msg, sizeof(pre_msg),
      "[PRE-START] state=%lu err=0x%lX\r\n",
      (uint32_t)HAL_CAN_GetState(&hcan1),
      HAL_CAN_GetError(&hcan1));
  HAL_UART_Transmit(&huart2, (uint8_t*)pre_msg, pre_len, 100);

  start_status = HAL_CAN_Start(&hcan1);

  /* Start 후 상태 확인 */
  char post_msg[80];
  int post_len = snprintf(post_msg, sizeof(post_msg),
      "[POST-START] start=%d state=%lu err=0x%lX\r\n",
      start_status,
      (uint32_t)HAL_CAN_GetState(&hcan1),
      HAL_CAN_GetError(&hcan1));
  HAL_UART_Transmit(&huart2, (uint8_t*)post_msg, post_len, 100);

  uint32_t last_tick = 0;
  uint8_t led_state = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t now = HAL_GetTick();
	  if(now - last_tick >= 500){
		  last_tick = now;
		  led_state = !led_state;
		  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

		  /* -- TX --- */
		  CAN_TxHeaderTypeDef tx_header = {0};
		  tx_header.StdId	= 0x123;
		  tx_header.IDE 	= CAN_ID_STD;
		  tx_header.RTR 	= CAN_RTR_DATA;
		  tx_header.DLC 	= 8;
		  uint8_t tx_data[8];
		  memcpy(tx_data, &tx_count, 4); //카운터를 페이로드에 실음
		  memset(tx_data + 4, 0xAA, 4);

		  uint32_t mailbox;
		  HAL_StatusTypeDef tx_status = HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &mailbox); //반환값 저장
		  if(tx_status == HAL_OK){
			  tx_count++;
		  }

		  uint32_t err = HAL_CAN_GetError(&hcan1);  //에러코드 가져오기
		  uint32_t state = HAL_CAN_GetState(&hcan1); // 상태 가져오기

		  /* --- RX 폴링 --- */
		  if(HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0){
			  CAN_RxHeaderTypeDef rx_header;
			  uint8_t rx_data[8];
			  if(HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK){
				  rx_count++;
			  }
		  }

		  // UART 출력
		  char msg[128];
		  int len = snprintf(msg, sizeof(msg), "[F446RE] tx=%lu rx=%lu st=%d err=0x%lX state=%lu t=%lums\r\n",
				  tx_count, rx_count, tx_status, err, state, now);
		  HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
	  }

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
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

/* USER CODE BEGIN 4 */

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
