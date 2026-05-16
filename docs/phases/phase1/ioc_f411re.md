# STM32CubeMX .ioc Configuration — F411RE Phase 1 (phase1_loopback)

Phase 1 목표: F411RE SPI2를 통해 MCP2515에 접근하고, MCP2515를 **내부 루프백 모드**로 동작시켜 TX 버퍼에 넣은 프레임이 RX 버퍼에 그대로 도착하는지 검증한다.
트랜시버(TJA1050) 활성화 없음. CAN_H / CAN_L 연결 없음. SPI ↔ MCP2515 내부에서 완결되는 테스트.

---

## 이 단계의 의의

F411RE에는 bxCAN 페리퍼럴이 없다. Phase 2에서 CAN 버스에 참여하기 위한 경로는:

```
F411RE SPI2 ──→ MCP2515 (CAN 컨트롤러) ──→ TJA1050 (트랜시버, 모듈 내장) ──→ CAN 버스
```

이 경로에는 검증이 필요한 독립적 레이어가 두 개 있다:

1. **SPI 통신 레이어**: F411RE ↔ MCP2515 레지스터 읽기/쓰기
2. **MCP2515 CAN 레이어**: MCP2515가 CAN 프레임을 TX 버퍼에서 RX 버퍼로 루프백할 수 있는가

Phase 1에서 이 두 레이어를 루프백으로 검증해 두면, Phase 2에서 추가되는 새 변수는 "물리 레이어(CAN_H/CAN_L 배선과 버스 종단)"뿐이다.

---

## F446RE Phase 1과의 핵심 차이

| 항목 | F446RE Phase 1 | **F411RE Phase 1** |
|------|---------------|-------------------|
| CAN 페리퍼럴 | bxCAN (내장) | **없음 — MCP2515(SPI 외부)** |
| 루프백 설정 | CubeMX `.ioc` Operating Mode=Loopback | **MCP2515 CANCTRL 레지스터 쓰기** |
| 비트 타이밍 | CubeMX Prescaler/BS1/BS2 | **MCP2515 CNF1/CNF2/CNF3 레지스터** |
| Phase 1에서 추가되는 페리퍼럴 | CAN1 | **SPI2 + GPIO(CS)** |
| 코드 복잡도 | HAL_CAN_* API | **직접 SPI 레지스터 접근** |

---

## 1. 새 프로젝트 생성

STM32CubeIDE → File → New → STM32 Project

| 항목 | 값 |
|------|----|
| Board Selector 탭 | `NUCLEO-F411RE` 검색 후 선택 |
| Project Name | `phase1_loopback` |
| Project Location | `firmware/f411re_node/phase1_loopback/` |
| Targeted Language | C |

> "Initialize all peripherals with their default Mode?" → **`No`**

---

## 2. RCC, SYS, GPIO, USART2 — Phase 0와 동일

Phase 0의 `phase0_alive`와 완전히 동일. [`phase0/ioc_f411re.md`](../phase0/ioc_f411re.md) 참조.

| 페리퍼럴 | 설정 |
|----------|------|
| RCC | HSE = BYPASS Clock Source |
| Clock Config | SYSCLK = 100 MHz, **APB1 = 50 MHz**, APB2 = 100 MHz |
| SYS | Debug = Serial Wire, Timebase = SysTick |
| GPIO PA5 | LD2, GPIO_Output, Low, Low speed, No pull |
| USART2 | Asynchronous, 115200 8N1, RX/TX 모두 |

---

## 3. SPI2 — MCP2515 인터페이스

`Connectivity → SPI2`

### Mode

| 항목 | 설정 |
|------|------|
| Mode | **Full-Duplex Master** |
| Hardware NSS Signal | **Disabled** |

Hardware NSS를 비활성화하는 이유: CS(Chip Select) 타이밍을 MCP2515 프로토콜에 맞게 수동으로 제어하기 위함. HAL NSS 자동 관리는 SPI 프레임 단위로만 동작하는데, MCP2515는 한 트랜잭션에서 여러 바이트를 CS 유지한 채로 보내야 한다.

### Parameter Settings

| 항목 | 설정 | 근거 |
|------|------|------|
| Frame Format | Motorola | SPI Mode 0,0 표준 |
| Data Size | **8 Bits** | MCP2515 SPI 단위 |
| First Bit | **MSB First** | MCP2515 요구사항 |
| Prescaler | **8** | 50 MHz / 8 = **6.25 MHz** ≤ MCP2515 최대 10 MHz |
| CPOL | **Low** | MCP2515: SCK idle = Low |
| CPHA | **1 Edge** | MCP2515: 첫 번째 엣지에서 샘플링 |
| CRC Calculation | Disabled | — |

### NVIC Settings

| 항목 | 설정 |
|------|------|
| 모든 SPI2 인터럽트 | **체크 해제** (폴링) |

### 핀 할당 확인 (Pinout view)

| 핀 | Signal |
|----|--------|
| **PB13** | SPI2_SCK |
| **PB14** | SPI2_MISO |
| **PB15** | SPI2_MOSI |

> **PB14 (MISO) 5V 내성:** MCP2515 모듈 MISO 출력은 5V 레벨. PB14는 STM32F411RE 데이터시트 Table 11에서 FT(Five-volt tolerant)로 분류된 핀이다. 로직 레벨 컨버터 없이 직결 가능.

---

## 4. CS 핀 — GPIO_Output

Pinout view에서 **PB6** 클릭 → `GPIO_Output` 선택.

| 항목 | 설정 |
|------|------|
| GPIO output level | **High** (비활성 초기값 — CS는 Active Low) |
| GPIO mode | Output Push Pull |
| GPIO Pull-up/Pull-down | No pull |
| Maximum output speed | Low |
| User Label | **MCP_CS** |

> CS 초기값이 Low라면, 프로젝트 시작 즉시 MCP2515로 쓰레기 클럭이 들어간다. **반드시 High(비활성)**로 설정할 것.

---

## 5. 활성화하지 말아야 할 것

| 항목 | 이유 |
|------|------|
| SPI1 | PA5(SPI1_SCK)는 LD2와 충돌. SPI2가 LED와 충돌 없이 사용 가능. |
| CAN | F411RE에 bxCAN 없음. |
| FreeRTOS | 폴링 루프로 충분. |
| DMA | SPI 폴링(HAL_SPI_Transmit/Receive)으로 충분. |
| TIM 인터럽트 | `HAL_GetTick()` 기반 주기 제어로 충분. |

---

## 6. Project Manager 탭

| 항목 | 값 |
|------|-----|
| Project Name | phase1_loopback |
| Project Location | `firmware/f411re_node/phase1_loopback/` |
| Toolchain / IDE | **STM32CubeIDE** |
| Generate peripheral init. as a pair of .c/.h files | ✅ |
| Keep user code when re-generating | ✅ |

---

## 7. 코드 생성 후 확인 사항

1. `Core/Src/spi.c`, `Core/Inc/spi.h` 생성됨 확인
2. `main.c`에 `MX_SPI2_Init()`, `MX_GPIO_Init()` 호출 확인
3. `SystemClock_Config()`에서 APB1 분주비 `RCC_HCLK_DIV2` 유지 확인
4. **빌드 → 에러 없이 `.elf` 생성**
5. **플래시 → LED 1Hz 점멸 + UART "alive" 그대로 출력** (SPI를 켰지만 USER CODE에서 호출 전이므로 Phase 0와 동일해야 함)

---

## 8. MCP2515 비트 타이밍 계산 (500 kbps @ 8 MHz 크리스탈)

MCP2515 모듈은 통상 **8 MHz 외부 크리스탈**을 사용한다. 모듈 기판의 크리스탈 패키지를 육안으로 확인할 것 (8.000 또는 16.000 표기).

> 아래 계산은 8 MHz 기준. 16 MHz 모듈이라면 BRP 값을 조정해야 한다 (BRP=1 → TQ=250ns 유지).

### 비트 타이밍 공식

```
TQ = 2 × (BRP + 1) / Fosc
비트레이트 = Fosc / (2 × (BRP + 1) × (1 + PropSeg + PhSeg1 + PhSeg2))
```

### 계산

```
Fosc = 8 MHz, 목표 = 500 kbps
TQ = 2 × (0 + 1) / 8,000,000 = 250 ns   (BRP = 0)
비트 시간 = 1 / 500,000 = 2,000 ns = 8 TQ

분배 (합계 = 8 TQ):
  Sync      = 1 TQ (고정)
  PropSeg   = 2 TQ
  PhSeg1    = 3 TQ
  PhSeg2    = 2 TQ
  ─────────────────
  합계      = 8 TQ ✓

샘플 포인트 = (1 + 2 + 3) / 8 = 75%
```

### 레지스터 값

| 레지스터 | 값 | 해석 |
|---------|-----|------|
| **CNF1** | `0x00` | BRP=0, SJW=1 TQ |
| **CNF2** | `0x91` | BTLMODE=1, SAM=0, PhSeg1=3TQ(값=2), PropSeg=2TQ(값=1) |
| **CNF3** | `0x01` | PhSeg2=2TQ(값=1) |

CNF2 비트 분해:

```
0x91 = 1_0_010_001
       │ │ │   └── PRSEG[2:0] = 001 → PropSeg = 1+1 = 2 TQ
       │ │ └────── PHSEG1[2:0] = 010 → PhSeg1 = 2+1 = 3 TQ
       │ └──────── SAM = 0 (한 번 샘플링)
       └────────── BTLMODE = 1 (PhSeg2 길이를 CNF3로 결정)
```

---

## 9. USER CODE 작성 가이드

### MCP2515 상수 및 헬퍼 함수

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
/* MCP2515 SPI 명령어 */
#define MCP_RESET        0xC0
#define MCP_READ         0x03
#define MCP_WRITE        0x02
#define MCP_BIT_MODIFY   0x05
#define MCP_RTS_TX0      0x81

/* MCP2515 레지스터 주소 */
#define MCP_CNF3         0x28
#define MCP_CNF2         0x29
#define MCP_CNF1         0x2A
#define MCP_CANINTF      0x2C
#define MCP_CANCTRL      0x0F
#define MCP_CANSTAT      0x0E
#define MCP_TXB0CTRL     0x30
#define MCP_TXB0SIDH     0x31
#define MCP_TXB0SIDL     0x32
#define MCP_TXB0DLC      0x35
#define MCP_TXB0D0       0x36
#define MCP_RXB0CTRL     0x60
#define MCP_RXB0SIDH     0x61
#define MCP_RXB0DLC      0x65
#define MCP_RXB0D0       0x66

/* MCP_CANCTRL 모드 비트[7:5] */
#define MCP_MODE_LOOPBACK 0x40
#define MCP_MODE_CONFIG   0x80

#define MCP_CS_LOW()   HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET)
#define MCP_CS_HIGH()  HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET)
/* USER CODE END PD */

/* USER CODE BEGIN PV */
uint32_t tx_count = 0;
uint32_t rx_count = 0;
/* USER CODE END PV */
```

### 헬퍼 함수 (USER CODE BEGIN 0)

```c
/* USER CODE BEGIN 0 */
static void mcp2515_reset(void) {
    uint8_t cmd = MCP_RESET;
    MCP_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
    MCP_CS_HIGH();
    HAL_Delay(10);
}

static void mcp2515_write(uint8_t reg, uint8_t val) {
    uint8_t buf[3] = {MCP_WRITE, reg, val};
    MCP_CS_LOW();
    HAL_SPI_Transmit(&hspi2, buf, 3, 10);
    MCP_CS_HIGH();
}

static uint8_t mcp2515_read(uint8_t reg) {
    uint8_t tx[3] = {MCP_READ, reg, 0xFF};
    uint8_t rx[3] = {0};
    MCP_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 10);
    MCP_CS_HIGH();
    return rx[2];
}

static void mcp2515_bit_modify(uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t buf[4] = {MCP_BIT_MODIFY, reg, mask, val};
    MCP_CS_LOW();
    HAL_SPI_Transmit(&hspi2, buf, 4, 10);
    MCP_CS_HIGH();
}

static void mcp2515_init_loopback(void) {
    mcp2515_reset();                            /* Config 모드 진입 */
    mcp2515_write(MCP_CNF1, 0x00);             /* BRP=0, SJW=1 */
    mcp2515_write(MCP_CNF2, 0x91);             /* PhSeg1=3TQ, PropSeg=2TQ */
    mcp2515_write(MCP_CNF3, 0x01);             /* PhSeg2=2TQ */
    mcp2515_write(MCP_RXB0CTRL, 0x60);         /* 필터 바이패스 (모든 프레임 수신) */
    mcp2515_write(MCP_CANCTRL, MCP_MODE_LOOPBACK);
    HAL_Delay(5);
}

static void mcp2515_send_frame(uint16_t id, uint8_t *data, uint8_t len) {
    mcp2515_write(MCP_TXB0SIDH, (id >> 3) & 0xFF);
    mcp2515_write(MCP_TXB0SIDL, (id << 5) & 0xE0);
    mcp2515_write(MCP_TXB0DLC,  len & 0x0F);
    for (uint8_t i = 0; i < len && i < 8; i++) {
        mcp2515_write(MCP_TXB0D0 + i, data[i]);
    }
    uint8_t cmd = MCP_RTS_TX0;
    MCP_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
    MCP_CS_HIGH();
}
/* USER CODE END 0 */
```

### 메인 초기화 및 루프 (USER CODE BEGIN 2, WHILE)

```c
/* USER CODE BEGIN 2 */
MCP_CS_HIGH();
HAL_Delay(10);

mcp2515_init_loopback();

uint8_t canstat = mcp2515_read(MCP_CANSTAT);
char init_msg[64];
int init_len = snprintf(init_msg, sizeof(init_msg),
                        "[F411RE] init %s (CANSTAT=0x%02X)\r\n",
                        ((canstat & 0xE0) == MCP_MODE_LOOPBACK) ? "OK" : "FAIL",
                        canstat);
HAL_UART_Transmit(&huart2, (uint8_t*)init_msg, init_len, 100);

uint32_t last_tick = 0;
uint8_t  led_state = 0;
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    uint32_t now = HAL_GetTick();
    if (now - last_tick >= 500) {
        last_tick = now;
        led_state = !led_state;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

        /* TX */
        uint8_t tx_data[4];
        memcpy(tx_data, &tx_count, 4);
        mcp2515_send_frame(0x123, tx_data, 4);
        tx_count++;

        /* TX 완료 대기 (TXB0CTRL.TXREQ 클리어 대기, 최대 5ms) */
        uint32_t t = HAL_GetTick();
        while ((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 5)) {}

        /* RX 폴링 */
        if (mcp2515_read(MCP_CANINTF) & 0x01) {     /* RX0IF 비트 */
            uint8_t rx_data[8];
            uint8_t rx_len = mcp2515_read(MCP_RXB0DLC) & 0x0F;
            for (uint8_t i = 0; i < rx_len && i < 8; i++) {
                rx_data[i] = mcp2515_read(MCP_RXB0D0 + i);
            }
            mcp2515_bit_modify(MCP_CANINTF, 0x01, 0x00);  /* RX0IF 클리어 */
            rx_count++;
        }

        /* UART 출력 */
        char msg[64];
        int len = snprintf(msg, sizeof(msg),
                           "[F411RE] tx=%lu rx=%lu t=%lums\r\n",
                           tx_count, rx_count, now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }
}
/* USER CODE END WHILE */
```

---

## 요약

```
활성화: Phase 0 + SPI2(Full-Duplex Master, CPOL=0, CPHA=0, 6.25MHz) + PB6(MCP_CS, 초기 High)
비활성화: CAN, FreeRTOS, DMA, 인터럽트, SPI1, 추가 타이머
검증: 빌드 OK + LED 1Hz + "init OK (CANSTAT=0x40)" + UART에서 tx/rx 카운터 동기 증가
```

Phase 1 F411RE sign-off 기준은 [`checklist_f411re.md`](checklist_f411re.md) 참조.
