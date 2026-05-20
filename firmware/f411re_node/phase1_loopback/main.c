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
#include "spi.h"
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
/* MCP2515 SPI 명령어 */
#define MCP_RESET		0xC0
#define MCP_READ		0x03
#define MCP_WRITE		0x02
#define MCP_BIT_MODIFY	0x05
#define MCP_RTS_TX0		0x81

/* MCP2515 레지스터 주소 */
#define MCP_CNF3		0x28
#define MCP_CNF2		0x29
#define MCP_CNF1		0x2A
#define MCP_CANINTF		0x2C
#define MCP_CANCTRL		0x0F
#define MCP_CANSTAT 	0x0E
#define	MCP_TXB0CTRL	0x30
#define MCP_TXB0SIDH	0x31
#define MCP_TXB0SIDL	0x32
#define MCP_TXB0DLC		0x35
#define MCP_TXB0D0		0x36
#define MCP_RXB0CTRL 	0x60
#define MCP_RXB0SIDH	0x61
#define MCP_RXB0DLC		0x65
#define MCP_RXB0D0		0x66

/* MCP_CANCTRL 모드 비트[7:5] */
#define MCP_MODE_LOOPBACK 	0x40
#define MCP_MODE_CONFIG 	0x80

#define MCP_CS_LOW() HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET)
#define MCP_CS_HIGH() HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET)
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

//mcp2515_reset() -> 0xC0 명령으로 MCP2515 소프트 리셋
static void mcp2515_reset(void){
	uint8_t cmd = MCP_RESET;
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
	MCP_CS_HIGH();
	HAL_Delay(10);
}

// mcp2515_write(reg, val) -> 레지스터 1바이트 쓰기
static void mcp2515_write(uint8_t reg, uint8_t val){
	uint8_t buf[3] = {MCP_WRITE, reg, val};
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, buf, 3, 10);
	MCP_CS_HIGH();
}

// mcp2515_read(reg) -> 레지스터 1바이트 읽기(더미 0xFF와 함께 3바이트 트랜잭션)
static uint8_t mcp2515_read(uint8_t reg){
	uint8_t tx[3] = {MCP_READ, reg, 0xFF};
	uint8_t rx[3] = {0};
	MCP_CS_LOW();
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 10);
	MCP_CS_HIGH();
	return rx[2];
}

// mcp2515_bit_modify(reg, mask, val) -> 마스크 지정 비트만 변경 (원자적 수정)
static void mcp2515_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){
	uint8_t buf[4] = {MCP_BIT_MODIFY, reg, mask, val};
	MCP_CS_LOW();
	HAL_SPI_Transmit(&hspi2, buf, 4, 10);
	MCP_CS_HIGH();
}

//초기화
static void mcp2515_init_loopback(void) {
    mcp2515_reset();                            /* Config 모드 진입 */
    mcp2515_write(MCP_CNF1, 0x00);             /* BRP=0, SJW=1 */
    mcp2515_write(MCP_CNF2, 0x91);             /* PhSeg1=3TQ, PropSeg=2TQ */
    mcp2515_write(MCP_CNF3, 0x01);             /* PhSeg2=2TQ */
    mcp2515_write(MCP_RXB0CTRL, 0x60);         /* 필터 바이패스 (모든 프레임 수신) */
    mcp2515_write(MCP_CANCTRL, MCP_MODE_LOOPBACK);
    HAL_Delay(5);
}

// 프레임 전송
static void mcp2515_send_frame(uint16_t id, uint8_t *data, uint8_t len){
	mcp2515_write(MCP_TXB0SIDH, (id >> 3) & 0xFF); // ID 상위 8 비트
	mcp2515_write(MCP_TXB0SIDL, (id << 5) & 0xE0); // ID 하위 3비트 -> [7:5]에 배치
	mcp2515_write(MCP_TXB0DLC, len & 0x0F); //데이터 길이
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

  mcp2515_init_loopback();

  /* CANSTAT 읽기 - Config 모드 (0X80)인지 확인 */
  uint8_t canstat = mcp2515_read(MCP_CANSTAT);
  char init_msg[64];
  int init_len = snprintf(init_msg, sizeof(init_msg), "[F411RE] init %s (CANSTAT=0x%02x)\r\n", ((canstat & 0xE0) == MCP_MODE_LOOPBACK) ? "OK" : "FAIL", canstat);

  HAL_UART_Transmit(&huart2, (uint8_t*)init_msg, init_len, 100);

  uint32_t last_tick = 0;
  uint8_t led_state = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 500 ms마다 반복
    // 1. LED 토글 - LD2 blink로 동작 확인
    // 2. TX - tx_count 값(4바이트)을 ID 0x123으로 전송
    // 3. TX 완료 대기 - TXB0CTRL.TXREQ(bit3) 클리어될 때까지 최대 5ms 폴링
    // 4. UART 출력 - [F411RE] tx=N rx=N t=Xms 형식으로 PC에서 시리얼 출력
    
	  uint32_t now = HAL_GetTick();
	  if(now - last_tick >= 500){
		  last_tick = now;
		  led_state = !led_state;
		  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

		  // TX
		  uint8_t tx_data[4];
		  memcpy(tx_data, &tx_count, 4);
		  mcp2515_send_frame(0x123, tx_data, 4);
		  tx_count++;

		  // TX 완료 대기 (TXB0CTRL.TXBEQ 클리어 대기, 최대 5ms)
		  uint32_t t = HAL_GetTick();
		  while((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 5)) {}

		  // RX 폴링
		  if(mcp2515_read(MCP_CANINTF) & 0x01){ //RX0IF 비트
			  uint8_t rx_data[8];
			  uint8_t rx_len = mcp2515_read(MCP_RXB0DLC) & 0x0F;
			  for (uint8_t i=0; i<rx_len && i<8; i++){
				  rx_data[i] = mcp2515_read(MCP_RXB0D0 + i);
			  }
			  mcp2515_bit_modify(MCP_CANINTF, 0x01, 0x00); //RX0IF 클리어
			  rx_count++;
		  }

		  //UART 출력
		  char msg[64];
		  int len = snprintf(msg, sizeof(msg),
		                     "[F411RE] tx=%lu rx=%lu t=%lums\r\n",
		                     tx_count, rx_count, now);
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
