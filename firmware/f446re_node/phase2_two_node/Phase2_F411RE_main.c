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
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// MCP2515 SPI 명령어
#define MCP_RESET 		0xC0
#define MCP_READ 		0x03
#define MCP_WRITE 		0x02
#define MCP_BIT_MODIFY 	0x05
#define MCP_RTS_TX0 	0x81

// MCP 2515 레지스터 주소
#define MCP_CNF3 		0x28
#define MCP_CNF2		0x29
#define MCP_CNF1		0x2A
#define MCP_CANINTF 	0x2C
#define MCP_EFLG		0x2D
#define MCP_CANCTRL		0x0F
#define MCP_CANSTAT 	0x0E
#define MCP_TXB0CTRL	0x30
#define MCP_TXB0SIDH 	0x31
#define MCP_TXB0SIDL	0x32
#define MCP_TXB0DLC		0x35
#define MCP_TXB0D0 		0x36
#define MCP_RXB0CTRL	0x60
#define MCP_RXB0SIDH 	0x61
#define MCP_RXB0SIDL	0x62
#define MCP_RXB0DLC		0x65
#define MCP_RXB0D0		0x66

// CANCTRL 모든 비트 [7:5]
#define MCP_MODE_NORMAL		0x00
#define MCP_MODE_LOOPBACK 	0x40
#define MCP_MODE_CONFIG 	0x80

#define MCP_CS_LOW()	HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET)
#define MCP_CS_HIGH()	HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint32_t self_tx_count = 0; // 내가 보낸 프레임 수 (0x11 기준)
uint32_t peer_rx_count = 0; //상대 (F446RE)에서 받은 0x010 프레임 수
static uint8_t sawtooth = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void mcp2515_reset(void){
	uint8_t cmd = MCP_RESET;
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
	MCP_CS_HIGH();
	HAL_Delay(10);
}

static void mcp2515_write(uint8_t reg, uint8_t val){
	uint8_t buf[3] = {MCP_WRITE, reg, val};
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, buf, 3, 10);
	MCP_CS_HIGH();
}

static uint8_t mcp2515_read(uint8_t reg){
	uint8_t tx[3] = {MCP_READ, reg, 0xFF};
	uint8_t rx[3] = {0};
	MCP_CS_LOW();
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 10);
	MCP_CS_HIGH();
	return rx[2];
}

static void mcp2515_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){
	uint8_t buf[4] = {MCP_BIT_MODIFY, reg, mask, val};
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, buf, 4, 10);
	MCP_CS_HIGH();
}

// Phase 2: Normal 모드 초기화
static void mcp2515_init_normal(void){
	mcp2515_reset();
	mcp2515_write(MCP_CNF1, 0x00); // BRP=0, SJW=1
	mcp2515_write(MCP_CNF2, 0x91); // PhSeg1=3TQ, PropSeg=2TQ
	mcp2515_write(MCP_CNF3, 0x01); // PhSeg2=2TQ -> 500 kbps @ 8MHz
	mcp2515_write(MCP_RXB0CTRL, 0x60); // RXB0: 필터 바이패스 (모든 프레임 수신)
	mcp2515_write(MCP_CANCTRL, MCP_MODE_NORMAL); // 핵심 변경: 0ㅌ00 = Normal
	HAL_Delay(5);
}

static void mcp2515_send_frame(uint16_t id, uint8_t *data, uint8_t len){
	mcp2515_write(MCP_TXB0SIDH, (id >> 3) & 0xFF);
	mcp2515_write(MCP_TXB0SIDL, (id << 5) & 0xE0);
	mcp2515_write(MCP_TXB0DLC, len & 0x0F);
	for(uint8_t i=0; i<len && i<8; i++){
		mcp2515_write(MCP_TXB0D0 + i, data[i]);
	}
	uint8_t cmd = MCP_RTS_TX0;
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
	MCP_CS_HIGH();
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
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  MCP_CS_HIGH();
  HAL_Delay(10);

  mcp2515_init_normal();

  uint8_t canstat = mcp2515_read(MCP_CANSTAT);
  char boot_msg[64];
  int boot_len = snprintf(boot_msg, sizeof(boot_msg),
		  "[F411RE] init %s (CANSTAT=0x%02X)\r\n",
		  ((canstat & 0xE0) == MCP_MODE_NORMAL) ? "OK" : "FAIL",
				  canstat);
  HAL_UART_Transmit(&huart2, (uint8_t*)boot_msg, boot_len, 100);

  uint32_t last_hb_tick 	= 0;
  uint32_t last_state_tick 	= 0;
  uint8_t led_state 		= 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t now = HAL_GetTick();

	  // 하트비트 TX (0x011, 100ms)
	  if(now - last_hb_tick >= 100){
		  last_hb_tick = now;

		  led_state = !led_state;
		  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

		  uint8_t tx_data[8] = {0};
		  memcpy(tx_data, &self_tx_count, 4);
		  tx_data[4] = 0x00;
		  uint16_t uptime_s = (uint16_t)(now / 1000);
		  tx_data[5] = uptime_s & 0xFF;
		  tx_data[6] = (uptime_s >> 8) & 0xFF;
		  tx_data[7] = 0x00;

		  mcp2515_send_frame(0x011, tx_data, 8);

		  // Tx 완료 대기 (최대 10ms): Normal 모드는 버스 점유
		  uint32_t t = HAL_GetTick();
		  while((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 10)) {}

		  // Tx 성공 여부: TXERR 비트(0x10) 없으면 성공
		  if(!(mcp2515_read(MCP_TXB0CTRL) & 0x10)) {
			  self_tx_count++;
		  }

		  // UART 상태 출력
		  uint8_t eflg = mcp2515_read(MCP_EFLG);
		  char msg[96];
		  int len = snprintf(msg, sizeof(msg),
				  "[F411RE] self_tx=%lu peer_rx=%lu eflg=0x%02X t=%lums\r\n",
				  self_tx_count, peer_rx_count, eflg, now);
		  HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
	  }

	  // 센서 데이터 Tx (0x200, 50ms)
	  if(now - last_state_tick >= 50) {
		  last_state_tick = now;

		  uint8_t tx_data[8];
		  memset(tx_data, sawtooth, 8);
		  sawtooth++;

		  mcp2515_send_frame(0x200, tx_data, 8);

		  uint32_t t = HAL_GetTick();
		  while((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 10)) {}
	  }

	  // RX 폴링
	  if(mcp2515_read(MCP_CANINTF) & 0x01){ //RX0IF
		  uint8_t rx_sidh = mcp2515_read(MCP_RXB0SIDH);
		  uint8_t rx_sidl = mcp2515_read(MCP_RXB0SIDL);
		  uint16_t rx_id = ((uint16_t)rx_sidh << 3) | (rx_sidl >> 5);

		  uint8_t rx_len = mcp2515_read(MCP_RXB0DLC) & 0x0F;
		  uint8_t rx_data[8];
		  for(uint8_t i=0; i<rx_len && i<8; i++){
			  rx_data[i] = mcp2515_read(MCP_RXB0D0 + i);
		  }
		  mcp2515_bit_modify(MCP_CANINTF, 0x01, 0x00); // RX0IF 클리어

		  if(rx_id == 0x010){ // F446RE 하트비트
			  peer_rx_count++;
		  }
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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MCP_CS_Pin */
  GPIO_InitStruct.Pin = MCP_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MCP_CS_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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
