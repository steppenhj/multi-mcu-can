# Phase 1 — F411RE MCP2515 SPI CAN 루프백 검증

> Phase 0 F411RE sign-off가 완료된 상태에서 시작한다. F446RE Phase 1 sign-off는 선행 조건이 아니다 — 두 노드는 독립적으로 검증한다. 운영 원칙 #3 ("한 번에 하나의 레이어만 추가한다")에 따라 Phase 2(버스 연결)를 먼저 시작하지 않는다.

```
PC (노트북, Tera Term)
        ▲
        │ USB (가상 COM 포트)
        │
   ST-Link
        ▲
        │ USART2 (PA2/PA3) — UART 통신
        │
   ┌────┴─────────────────────────────────────┐
   │  F411RE MCU                              │
   │                                          │
   │   main loop                              │
   │     │                                    │
   │     ├─ mcp2515_send_frame() ─────────┐   │
   │     │                                ▼   │
   │     │                         SPI2 출력  │
   │     │                                │   │
   └─────┼────────────────────────────────┼───┘
         │ SCK/MOSI/CS              MISO  │
         ▼                                │
   ┌─────────────────────────────────────┐│
   │  MCP2515 (SPI CAN 컨트롤러)         ││
   │                                     ││
   │   TX 버퍼 ──┐                        ││
   │             ▼ (루프백)               ││
   │        ┌─────────┐                  ││
   │        │ CAN 내부 │                  ││
   │        │ 루프백   │                  ││
   │        └────┬────┘                  ││
   │             ▼                       ││
   │   RX 버퍼 ◄─┘                        ││
   │             │                       ││
   └─────────────┼───────────────────────┘│
                 │ MISO (CANINTF.RX0IF)    │
                 └────────────────────────┘

   ※ TJA1050 트랜시버(모듈 내장)와 CAN_H/CAN_L은 미연결.
      MCP2515 내부에서 TX → RX가 직결되는 루프백 모드.
```

1. **UART (PC ↔ F411RE)** — 디버그 채널. tx_count, rx_count를 Tera Term으로 관찰.
2. **SPI (F411RE ↔ MCP2515)** — MCP2515 레지스터 접근 경로. 이 경로가 먼저 검증되어야 한다.
3. **MCP2515 내부 루프백** — 진짜 검증 대상. MCP2515가 자기가 만든 프레임을 자기가 받을 수 있는가?

---

## 이 단계의 의의

F411RE에는 bxCAN 페리퍼럴이 없다. Phase 2에서 CAN 버스에 참여하기 위한 경로는:

```
F411RE SPI2 ──→ MCP2515 (CAN 컨트롤러) ──→ TJA1050 (트랜시버, 모듈 내장) ──→ CAN 버스
```

이 경로에는 검증이 필요한 독립적 레이어가 세 개 있다:

- **SPI 통신 레이어**: F411RE ↔ MCP2515 레지스터 읽기/쓰기
- **MCP2515 초기화 레이어**: 비트 타이밍, 루프백 모드 설정
- **MCP2515 CAN 레이어**: MCP2515가 CAN 프레임을 TX 버퍼에서 RX 버퍼로 루프백할 수 있는가

이 세 레이어를 Phase 2의 물리 버스와 함께 한 번에 검증하면, 실패 시 "SPI가 안 됩니까, MCP2515 설정이 잘못됩니까, 버스 배선이 잘못됩니까"를 동시에 뒤져야 한다. Phase 1에서 미리 잡으면 Phase 2에서 추가되는 변수는 "물리 레이어" 하나뿐이다.

### F446RE Phase 1과의 핵심 차이

| 항목 | F446RE Phase 1 | **F411RE Phase 1** |
|------|---------------|-------------------|
| CAN 페리퍼럴 | bxCAN (내장) | **없음 — MCP2515(SPI 외부)** |
| 루프백 설정 | CubeMX `.ioc` Operating Mode=Loopback | **MCP2515 CANCTRL 레지스터 쓰기** |
| 비트 타이밍 | CubeMX Prescaler/BS1/BS2 | **MCP2515 CNF1/CNF2/CNF3 레지스터** |
| Phase 1에서 추가되는 페리퍼럴 | CAN1 | **SPI2 + GPIO(CS)** |
| 코드 복잡도 | HAL_CAN_* API | **직접 SPI 레지스터 접근** |

---

## 필요 장비

- F411RE 보드 한 대 (Phase 0 sign-off 완료된 보드)
- MCP2515 + TJA1050 모듈 (아이템 34)
- 점퍼 케이블 6선: SCK, MOSI, MISO, CS, 5V, GND
- ST-Link 가상 COM 포트로 접속 가능한 시리얼 모니터 (Tera Term 등)
- 멀티미터 (배선 연속성 확인용)

**명시적 제외:** CAN_H / CAN_L 배선, 종단 저항, SN65HVD230, F446RE, RPi5. 단일 보드 + 단일 모듈.

---

## Step 1 — Phase 0 베이스라인 재확인

MCP2515 배선 연결 전, F411RE 단독 동작을 먼저 확인한다.

- [ ] `phase0_alive` 펌웨어 플래시. LED가 1Hz로 점멸하는지 확인.
- [ ] 시리얼 모니터(115200 8N1) 연결. `[F411RE] alive, t=Xms` 메시지가 1초마다 출력되는지 확인.
- [ ] `t=` 값이 단조 증가하는지 확인.
- [ ] USB 분리 → 재연결. 깨끗한 부팅 확인.

Phase 0 베이스라인이 깨져 있다면 여기서 멈추고 Phase 0부터 다시 검증한다.

---

## Step 2 — SPI 배선

MCP2515 모듈에 전원을 공급하고 F411RE SPI2와 연결한다.

### 전압 주의

| 항목 | 내용 |
|------|------|
| MCP2515 모듈 VCC | **5V** (NUCLEO 5V 헤더에서 공급) |
| MCP2515 모듈 GND | F411RE GND와 공통 |
| MOSI/SCK/CS 방향 | F411RE(3.3V) → MCP2515(5V): MCP2515 VIH ≈ 2.1V, 문제없음 |
| MISO 방향 | MCP2515(5V) → F411RE(3.3V): **PB14는 5V 내성 핀** (STM32F411 데이터시트 Table 11, FT 표시) |

### 배선표

| F411RE 핀 | MCP2515 모듈 핀 | 신호 |
|-----------|----------------|------|
| **5V** (CN6-4 또는 CN10-8) | VCC | 전원 |
| **GND** | GND | 기준 전위 |
| **PB13** | SCK | SPI2 클럭 |
| **PB14** | SO (MISO) | MCP2515 → F411RE |
| **PB15** | SI (MOSI) | F411RE → MCP2515 |
| **PB6** | CS | 칩 선택 (Active Low) |

> INT 핀(MCP2515 인터럽트 출력)은 Phase 1에서 연결하지 않는다. 폴링으로 진행하므로 불필요. Phase 4(스케줄링) 이후 필요 시 연결.

### 배선 후 멀티미터 확인

- [ ] PB13 ↔ SCK 연속성 확인 (< 1Ω).
- [ ] PB14 ↔ SO 연속성 확인.
- [ ] PB15 ↔ SI 연속성 확인.
- [ ] PB6 ↔ CS 연속성 확인.
- [ ] F411RE GND ↔ MCP2515 GND 연속성 확인.
- [ ] MCP2515 모듈 VCC에 5V 인가 시 모듈이 따뜻해지지 않음 확인 (배선 단락 없음).

---

## Step 3 — CubeMX 설정

### 3-1. 새 프로젝트 생성

STM32CubeIDE → File → New → STM32 Project

| 항목 | 값 |
|------|----|
| Board Selector 탭 | `NUCLEO-F411RE` 검색 후 선택 |
| Project Name | `phase1_loopback` |
| Project Location | `firmware/f411re_node/phase1_loopback/` |
| Targeted Language | C |

- [ ] STM32CubeIDE → New → STM32 Project → `NUCLEO-F411RE`
- [ ] Project Name: `phase1_loopback`. Location: `firmware/f411re_node/phase1_loopback/`
- [ ] "Initialize all peripherals with their default Mode?" → **`No`**

### 3-2. 클럭 / GPIO / USART2 — Phase 0와 동일

Phase 0의 `phase0_alive`와 완전히 동일. [`phase0/ioc_f411re.md`](../phase0/ioc_f411re.md) 참조.

| 페리퍼럴 | 설정 |
|----------|------|
| RCC | HSE = BYPASS Clock Source |
| Clock Config | SYSCLK = 100 MHz, **APB1 = 50 MHz**, APB2 = 100 MHz |
| SYS | Debug = Serial Wire, Timebase = SysTick |
| GPIO PA5 | LD2, GPIO_Output, Low, Low speed, No pull |
| USART2 | Asynchronous, 115200 8N1, RX/TX 모두 |

- [ ] RCC: HSE = **BYPASS Clock Source**
- [ ] SYSCLK = **100 MHz**, APB1 = **50 MHz** (APB1 위반 빨간색 없음)
- [ ] PA5 = LD2 (GPIO_Output), USART2 = 115200 8N1

### 3-3. SPI2 활성화

`Connectivity → SPI2`

#### Mode

| 항목 | 설정 |
|------|------|
| Mode | **Full-Duplex Master** |
| Hardware NSS Signal | **Disabled** |

Hardware NSS를 비활성화하는 이유: CS(Chip Select) 타이밍을 MCP2515 프로토콜에 맞게 수동으로 제어하기 위함. HAL NSS 자동 관리는 SPI 프레임 단위로만 동작하는데, MCP2515는 한 트랜잭션에서 여러 바이트를 CS 유지한 채로 보내야 한다.

#### Parameter Settings

| 항목 | 설정 | 근거 |
|------|------|------|
| Frame Format | Motorola | SPI Mode 0,0 표준 |
| Data Size | **8 Bits** | MCP2515 SPI 단위 |
| First Bit | **MSB First** | MCP2515 요구사항 |
| Prescaler | **8** | 50 MHz / 8 = **6.25 MHz** ≤ MCP2515 최대 10 MHz |
| CPOL | **Low** | MCP2515: SCK idle = Low |
| CPHA | **1 Edge** | MCP2515: 첫 번째 엣지에서 샘플링 |
| CRC Calculation | Disabled | — |

#### NVIC Settings

| 항목 | 설정 |
|------|------|
| 모든 SPI2 인터럽트 | **체크 해제** (폴링) |

#### 핀 할당 확인 (Pinout view)

| 핀 | Signal |
|----|--------|
| **PB13** | SPI2_SCK |
| **PB14** | SPI2_MISO |
| **PB15** | SPI2_MOSI |

> **PB14 (MISO) 5V 내성:** MCP2515 모듈 MISO 출력은 5V 레벨. PB14는 STM32F411RE 데이터시트 Table 11에서 FT(Five-volt tolerant)로 분류된 핀이다. 로직 레벨 컨버터 없이 직결 가능.

- [ ] `Connectivity → SPI2` → Mode: **Full-Duplex Master**
- [ ] Hardware NSS: **Disabled** (CS를 수동 GPIO로 관리)
- [ ] Data Size: 8 Bits, First Bit: MSB First
- [ ] CPOL: **Low**, CPHA: **1 Edge** (SPI Mode 0,0 — MCP2515 요구)
- [ ] Prescaler: **8** (SPI 클럭 = 50 MHz / 8 = **6.25 MHz** ≤ MCP2515 최대 10 MHz ✓)
- [ ] DMA, 인터럽트 모두 **비활성화** (폴링)
- [ ] Pinout view: PB13=SPI2_SCK, PB14=SPI2_MISO, PB15=SPI2_MOSI 자동 할당 확인

### 3-4. CS 핀 (PB6) — GPIO_Output

Pinout view에서 **PB6** 클릭 → `GPIO_Output` 선택.

| 항목 | 설정 |
|------|------|
| GPIO output level | **High** (비활성 초기값 — CS는 Active Low) |
| GPIO mode | Output Push Pull |
| GPIO Pull-up/Pull-down | No pull |
| Maximum output speed | Low |
| User Label | **MCP_CS** |

> CS 초기값이 Low라면, 프로젝트 시작 즉시 MCP2515로 쓰레기 클럭이 들어간다. **반드시 High(비활성)**로 설정할 것.

- [ ] PB6 클릭 → `GPIO_Output` 선택
- [ ] 라벨: `MCP_CS`
- [ ] Output Level: **High** (기본 비활성 상태)
- [ ] Speed: Low, Pull: No pull

### 3-5. 활성화하지 말아야 할 것

| 항목 | 이유 |
|------|------|
| SPI1 | PA5(SPI1_SCK)는 LD2와 충돌. SPI2가 LED와 충돌 없이 사용 가능. |
| CAN | F411RE에 bxCAN 없음. |
| FreeRTOS | 폴링 루프로 충분. |
| DMA | SPI 폴링(HAL_SPI_Transmit/Receive)으로 충분. |
| TIM 인터럽트 | `HAL_GetTick()` 기반 주기 제어로 충분. |

### 3-6. Project Manager 탭

| 항목 | 값 |
|------|-----|
| Project Name | phase1_loopback |
| Project Location | `firmware/f411re_node/phase1_loopback/` |
| Toolchain / IDE | **STM32CubeIDE** |
| Generate peripheral init. as a pair of .c/.h files | ✅ |
| Keep user code when re-generating | ✅ |

- [ ] Toolchain / IDE = **STM32CubeIDE**
- [ ] Generate peripheral init. as a pair of .c/.h files = **체크**
- [ ] Keep user code when re-generating = **체크**

### 3-7. 코드 생성 및 빌드 베이스라인

코드 생성 후 확인 사항:
1. `Core/Src/spi.c`, `Core/Inc/spi.h` 생성됨 확인
2. `main.c`에 `MX_SPI2_Init()`, `MX_GPIO_Init()` 호출 확인
3. `SystemClock_Config()`에서 APB1 분주비 `RCC_HCLK_DIV2` 유지 확인

- [ ] Generate Code → `Core/Src/spi.c`, `Core/Inc/spi.h` 생성됨 확인
- [ ] **빌드 → 에러 없이 `.elf` 생성**
- [ ] **플래시 → LED 1Hz 점멸 + UART "alive" 그대로 출력** (MCP2515 아직 미호출이므로 Phase 0와 동일)

> 이 시점이 Phase 1 진입 직전의 known-good 상태다. SPI를 켰지만 USER CODE에서 호출하지 않았으므로 동작은 Phase 0와 같아야 한다.

---

## Step 4 — MCP2515 비트 타이밍 계산 (500 kbps @ 8 MHz 크리스탈)

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

## Step 5 — MCP2515 기본 SPI 통신 확인

루프백 초기화 전, SPI 경로가 살아있는지 먼저 확인한다.

**검증 방법:** MCP2515를 RESET한 후 CANSTAT 레지스터(0x0E)를 읽는다. RESET 후 MCP2515는 Config 모드(0x80)로 진입하므로 CANSTAT 상위 3비트가 `100b`이면 SPI 통신이 정상이다.

```c
/* USER CODE BEGIN 2 */
/* CS 초기 High 보장 */
HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET);
HAL_Delay(10);

/* RESET */
mcp2515_reset();

/* CANSTAT 읽기 — Config 모드(0x80)인지 확인 */
uint8_t canstat = mcp2515_read(MCP_CANSTAT);
char msg[64];
int len = snprintf(msg, sizeof(msg), "[F411RE] CANSTAT=0x%02X (expect 0x80)\r\n", canstat);
HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
/* USER CODE END 2 */
```

- [ ] 플래시 후 Tera Term에서 `CANSTAT=0x80` 출력 확인
- [ ] 다른 값(0x00, 0xFF 등)이 나오면 SPI 배선 문제 — Step 2 배선으로 돌아간다

> **0x00이 나온다면:** SPI MISO 선이 끊겼거나 CS가 항상 Low. **0xFF이 나온다면:** MISO 선이 끊기고 풀업 상태. **0x80이 나온다면:** SPI 통신 정상.

---

## Step 6 — MCP2515 루프백 초기화 + USER CODE

### MCP2515 상수 및 전역 변수

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

체크리스트:

- [ ] MCP2515 상수 및 헬퍼 함수 추가 (`mcp2515_reset`, `mcp2515_read`, `mcp2515_write`, `mcp2515_bit_modify`)
- [ ] `mcp2515_init_loopback()` 호출: 비트 타이밍 설정 + 루프백 모드 진입
- [ ] 초기화 후 CANSTAT 상위 3비트 = `010b`(0x40 마스크) 확인 — 루프백 모드 진입 검증
- [ ] 메인 루프에 500ms 주기 TX → RX 폴링 → UART 출력 작성
- [ ] 빌드 → 경고 0개 또는 무시 가능한 경고만

---

## Step 7 — UART 출력 검증

- [ ] 플래시 → 보드 부팅
- [ ] LED가 1Hz로 점멸 (Phase 0와 동일한 시각적 신호)
- [ ] 시리얼 모니터에서 다음 형식의 메시지 출력 확인:

```
[F411RE] init OK (CANSTAT=0x40)
[F411RE] tx=1 rx=1 t=512ms
[F411RE] tx=2 rx=2 t=1024ms
[F411RE] tx=3 rx=3 t=1536ms
...
```

- [ ] **`tx`와 `rx` 값이 항상 같은 속도로 증가** (1씩, 같은 줄에서)
- [ ] `tx`가 증가하는데 `rx`가 0에 머물거나 뒤처지지 않음
- [ ] `t=` 값이 단조 증가

---

## Step 8 — 30초 안정성 관찰

- [ ] 30초 이상 관찰. tx/rx 차이 발생하지 않음
- [ ] MCP2515 모듈이 비정상적으로 따뜻하지 않음
- [ ] UART 메시지가 중단되거나 글자가 깨지지 않음

---

## 사인오프

위의 모든 체크박스가 표시되고, 30초 이상의 안정적인 tx==rx 동작이 관찰되면 Phase 1 F411RE가 완료된다.

- [ ] Tera Term 출력 30초 분량을 `docs/assets/captures/F411RE_Phase1.png` (스크린샷) 또는 텍스트로 보관
- [ ] `git add firmware/f411re_node/phase1_loopback/` 후 커밋. 예: `phase1(f411re): MCP2515 SPI loopback verified, tx==rx at 500kbps`

**이 커밋이 존재하기 전까지 Phase 2(F446RE ↔ F411RE 2노드 버스)를 시작할 수 없다.**

---

## 주의해야 할 실패 모드

| 증상 | 예상 원인 | 조치 |
|------|-----------|------|
| `CANSTAT=0x00` 또는 `0xFF` | SPI 통신 실패 (배선 단락/단선) | CS, MOSI, MISO, SCK 연속성 재확인. CS가 Active Low인지 확인(PB6 초기값 High). |
| `CANSTAT=0x80` (Config mode 유지) | `mcp2515_init_loopback()`에서 CANCTRL 쓰기 실패, 또는 모드 전환 지연 | CANCTRL 레지스터 쓰기 후 5ms 대기 확인. CANSTAT 재확인. |
| `init OK` 출력 후 `tx=1, rx=0` | 비트 타이밍 설정 오류 또는 RXB0CTRL 필터 설정 누락 | CNF1/CNF2/CNF3 값 재확인 (Step 4 참조). RXB0CTRL=0x60 (필터 바이패스) 확인. |
| `tx=0` (전혀 증가 안 함) | mcp2515_send_frame 내부에서 TXB0CTRL 쓰기 실패 | CANSTAT이 0x40(루프백)인지 재확인. Config 모드에서는 송수신 안 됨. |
| MCP2515 모듈이 즉시 뜨거워짐 | 전원 단락 또는 잘못된 VCC 연결 | **즉시 전원 차단**. 5V와 GND 배선 재확인. |
| MISO 읽기값이 항상 동일 (0x00 또는 0xFF) | MISO 선 단선 또는 F411RE 핀이 GPIO_Output으로 잘못 설정됨 | PB14가 CubeMX에서 SPI2_MISO(AF5)로 설정됐는지 확인. |
| SPI 클럭이 10MHz 초과 | Prescaler 계산 오류 | APB1=50MHz, Prescaler=8 → 6.25MHz 재확인. |

---

## 요약

```
활성화: Phase 0 + SPI2(Full-Duplex Master, CPOL=0, CPHA=0, 6.25MHz) + PB6(MCP_CS, 초기 High)
비활성화: CAN, FreeRTOS, DMA, 인터럽트, SPI1, 추가 타이머
검증: 빌드 OK + LED 1Hz + "init OK (CANSTAT=0x40)" + UART에서 tx/rx 카운터 동기 증가
```
