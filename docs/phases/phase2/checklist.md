# Phase 2 — F446RE ↔ F411RE 2노드 CAN 버스 검증

> Phase 1 F446RE sign-off **및** Phase 1 F411RE sign-off 커밋이 모두 존재할 때 시작한다. 두 커밋이 없으면 여기서 멈추고 Phase 1을 먼저 끝낸다.

---

## 이 단계가 검증하는 것

Phase 2의 유일한 질문: **두 노드가 물리 CAN 버스를 통해 서로의 프레임을 주고받을 수 있는가?**

Phase 1에서 이미 확인된 사실:
- F446RE bxCAN 페리퍼럴은 500 kbps 비트 타이밍이 정확하고, TX→RX 경로가 동작한다.
- F411RE MCP2515는 SPI 통신이 정상이고, 500 kbps 비트 타이밍이 정확하며, TX→RX 경로가 동작한다.

Phase 2에서 **새로 추가되는 변수는 물리 레이어 하나뿐**이다:
- SN65HVD230 트랜시버 연결 (F446RE 측)
- CAN_H / CAN_L 배선
- 종단 저항

물리 레이어만 분리되어 추가되므로, 실패하면 배선만 보면 된다.

---

## 시스템 구성도

```
PC (Tera Term A)              PC (Tera Term B)
      ▲                              ▲
      │ USB                          │ USB
 ST-Link A                      ST-Link B
      ▲                              ▲
      │ USART2                       │ USART2
┌─────┴──────────┐           ┌───────┴──────────┐
│  F446RE MCU    │           │  F411RE MCU       │
│                │           │                   │
│  bxCAN CAN1   │           │  SPI2 ──→ MCP2515 │
│  PA12 (TX) ───┼──┐     ┌──┼─ PB6 (CS)         │
│  PA11 (RX) ←──┼┐ │     │  │  PB13 (SCK)       │
└────────────────┘│ │     │  │  PB15 (MOSI)      │
                  │ │     │  │  PB14 (MISO)      │
         ┌────────┘ │     │  └────────────────────┘
         ▼          │     │           │
  ┌─────────────┐   │     │    ┌──────┴──────┐
  │ SN65HVD230  │   │     │    │  TJA1050    │
  │ (3.3V)      │   │     │    │  (5V, 내장) │
  │ TXD ←── PA12│   │     │    └─────────────┘
  │ RXD ──→ PA11│◄──┘     │           │
  │ CANH ───────┼──────────┼───── CANH │
  │ CANL ───────┼──────────┼───── CANL │
  └─────────────┘   │     │    └───────┘
       [120Ω]        │     │    [120Ω ← 모듈 내장 or 외장]
  (CANH-CANL 사이)   │     │
                     └─────┘
                  (공통 GND 필수)
```

### 메시지 흐름 (can_protocol.md 기준)

| ID | 송신 노드 | 주기 | 용도 |
|----|-----------|------|------|
| `0x010` | F446RE | 100ms | 하트비트 (counter, uptime) |
| `0x011` | F411RE | 100ms | 하트비트 (counter, uptime) |
| `0x100` | F446RE | 50ms | 상태 데이터 (톱니파 패턴) |
| `0x200` | F411RE | 50ms | 센서 데이터 (톱니파 패턴) |

---

## 필요 장비

- F446RE 보드 (Phase 1 sign-off 완료)
- F411RE 보드 (Phase 1 sign-off 완료)
- SN65HVD230 트랜시버 모듈 (하드웨어 #40, 3.3V)
- MCP2515 + TJA1050 모듈 (하드웨어 #34, 이미 F411RE에 배선된 상태)
- 점퍼 케이블: F446RE ↔ SN65HVD230 (4선: PA12, PA11, 3.3V, GND), 버스 배선 2선 (CAN_H, CAN_L)
- 브레드보드 400홀 (하드웨어 #38)
- 120Ω 저항 × 2 (하드웨어 #37, 모듈 내장 여부 확인 후 필요 시만)
- 멀티미터 (하드웨어 #39)
- PC 시리얼 모니터 2개 (Tera Term 창 2개, 또는 두 PC)

---

## Step 1 — Phase 1 베이스라인 재확인

배선 작업 전, 양쪽 노드가 여전히 Phase 1 상태에서 정상 동작하는지 확인한다.

**F446RE:**
- [ ] `phase1_loopback` 펌웨어 플래시.
- [ ] LED 1Hz 점멸, UART에서 `tx==rx` 카운터 동기 증가 확인.
- [ ] 30초 이상 관찰. 이상 없음.

**F411RE:**
- [ ] `phase1_loopback` 펌웨어 플래시.
- [ ] `[F411RE] init OK (CANSTAT=0x40)` 출력 확인.
- [ ] LED 1Hz 점멸, `tx==rx` 카운터 동기 증가 확인.

두 노드 중 하나라도 Phase 1 동작이 깨져 있다면, 여기서 멈추고 해당 Phase 1을 먼저 복구한다.

---

## Step 2 — 물리 버스 배선

### Step 2-1: SN65HVD230 모듈 핀맵 확인

SN65HVD230 모듈 실물의 핀 레이블을 확인한다. 모듈마다 표기가 다를 수 있으므로 아래 신호명 기준으로 찾는다:

| 모듈 핀 레이블 예시 | 신호 | 설명 |
|-------------------|------|------|
| VCC / 3V3 | VCC | 3.3V 전원 |
| GND | GND | 기준 전위 |
| CTX / TXD / D | TXD | MCU → 트랜시버 (CAN TX) |
| CRX / RXD / R | RXD | 트랜시버 → MCU (CAN RX) |
| CANH | CANH | CAN 버스 양성 |
| CANL | CANL | CAN 버스 음성 |
| RS / S / Slope | RS | 경사율 제어 (GND = 고속 모드) |

> **RS 핀:** 모듈에 RS 핀이 노출되어 있다면 GND에 연결한다 (고속 모드). 모듈 PCB에 RS가 이미 GND로 묶여 있다면 연결 불필요. 데이터시트나 모듈 회로도 확인.

---

### Step 2-2: F446RE → SN65HVD230 배선

| F446RE 핀 | 커넥터 위치 | SN65HVD230 핀 | 신호 | 점퍼 색 |
|-----------|------------|---------------|------|---------|
| **PA12** | CN10 | TXD | CAN1_TX | 노랑 |
| **PA11** | CN10 | RXD | CAN1_RX | 초록 |
| **3.3V** | CN6 pin 4 또는 CN10 | VCC | 전원 | 빨강 |
| **GND** | CN6 pin 6 또는 CN10 | GND | 기준 전위 | 검정 |
| (선택) | — | RS → GND | 고속 모드 (필요 시) | 흰색 |

- [ ] PA12 ↔ SN65HVD230 TXD 연결 (노랑)
- [ ] PA11 ↔ SN65HVD230 RXD 연결 (초록)
- [ ] 3.3V ↔ SN65HVD230 VCC 연결 (빨강)
- [ ] GND ↔ SN65HVD230 GND 연결 (검정)
- [ ] (해당 시) RS ↔ GND 연결 (흰색)

멀티미터 연속성 확인:
- [ ] PA12 ↔ TXD 연속성 < 1Ω
- [ ] PA11 ↔ RXD 연속성 < 1Ω
- [ ] F446RE GND ↔ SN65HVD230 GND 연속성 확인
- [ ] 전원 인가 후 SN65HVD230 모듈이 따뜻해지지 않음 (배선 단락 없음)

---

### Step 2-3: CAN 버스 배선 (CANH / CANL)

브레드보드를 중간 허브로 사용해 두 노드의 CAN_H / CAN_L을 연결한다.

```
SN65HVD230 CANH ──── 브레드보드 CANH 열 ──── MCP2515 모듈 CANH  (주황)
SN65HVD230 CANL ──── 브레드보드 CANL 열 ──── MCP2515 모듈 CANL  (파랑)
```

| 신호 | 점퍼 색 |
|------|---------|
| CAN_H | 주황 |
| CAN_L | 파랑 |
| GND 공통 | 검정 |

- [ ] SN65HVD230 CANH ↔ MCP2515 모듈 CANH 연결 (브레드보드 경유, 주황)
- [ ] SN65HVD230 CANL ↔ MCP2515 모듈 CANL 연결 (브레드보드 경유, 파랑)
- [ ] **F446RE GND ↔ F411RE GND 공통 연결 (검정)** ← 이것이 없으면 차동 신호 기준이 달라져 통신 불가

> GND 공통 연결은 선택 사항이 아니다. CAN 버스는 차동 신호지만, 두 노드의 GND가 분리되어 있으면 CANH/CANL 전압 레퍼런스가 달라져 수신 측이 비트를 오독한다. 반드시 연결할 것.

---

### Step 2-4: 종단 저항 확인 및 배치

CAN 버스 양쪽 물리적 끝단에 120Ω이 존재해야 한다. 병렬 합성 60Ω이 버스 특성 임피던스.

**MCP2515 모듈 내장 저항 확인:**

MCP2515+TJA1050 모듈(#34) PCB를 육안으로 확인한다. 대부분의 중국산 모듈에는 CANH-CANL 사이에 120Ω 저항이 납땜되어 있다 (레이블: R2 또는 120R).

- [ ] MCP2515 모듈 PCB에서 CANH-CANL 사이 120Ω 저항 유무 확인
- [ ] 멀티미터 저항 모드: MCP2515 CANH ↔ CANL 측정 → 120Ω ± 5% 이면 내장됨

**SN65HVD230 모듈 내장 저항 확인:**

동일하게 SN65HVD230 모듈 PCB 확인.

- [ ] SN65HVD230 모듈 PCB에서 CANH-CANL 사이 저항 유무 확인

**외부 저항 배치 결정:**

| 상황 | 조치 |
|------|------|
| 양쪽 모두 내장 없음 | 외부 120Ω × 2 (브레드보드, 각 노드 CAN_H-CAN_L 사이) |
| 한쪽만 내장 | 없는 쪽에만 외부 120Ω × 1 추가 |
| 양쪽 모두 내장 | 외부 저항 불필요 (이미 60Ω 병렬) |

- [ ] 종단 저항 배치 완료
- [ ] 멀티미터로 버스 전체 CANH-CANL 저항 측정: **약 60Ω** (두 120Ω 병렬)

> 전원이 연결된 상태에서 측정하면 안 된다. 트랜시버 내부 임피던스가 간섭하므로, **보드 전원 차단 후** 측정한다. 60Ω ± 10%가 정상.

---

## Step 3 — F446RE Phase 2 펌웨어

Phase 1 프로젝트를 수정하지 않는다. 새 프로젝트를 `firmware/f446re_node/phase2_normal/`에 생성한다.

### Step 3-1: 새 프로젝트 생성

Phase 1과 동일한 절차:
- [ ] STM32CubeIDE → New → STM32 Project → NUCLEO-F446RE
- [ ] Project Name: `phase2_normal`, Location: `firmware/f446re_node/phase2_normal/`
- [ ] "Initialize all peripherals with their default Mode?" → **No**

### Step 3-2: 클럭 / GPIO / USART2 설정 (Phase 1과 동일)

Phase 1과 완전히 동일:
- [ ] RCC: HSE = **BYPASS Clock Source**
- [ ] SYSCLK = **180 MHz**, APB1 = **45 MHz**, APB2 = 90 MHz
- [ ] PA5 = LD2 (GPIO_Output), USART2 = 115200 8N1
- [ ] SYS: Debug = Serial Wire, Timebase = SysTick

### Step 3-3: CAN1 설정 — Phase 1과 달라지는 부분만

Phase 1 설정을 기준으로, **변경되는 항목만** 강조한다:

| 항목 | Phase 1 | **Phase 2 변경값** |
|------|---------|------------------|
| **Operating Mode** | Loopback | **Normal** |
| **Automatic Retransmission** | Disable | **Enable** |
| Prescaler | 9 | 9 (동일) |
| BS1 | 8 TQ | 8 TQ (동일) |
| BS2 | 1 TQ | 1 TQ (동일) |
| SJW | 1 TQ | 1 TQ (동일) |
| NVIC | 전부 체크 해제 | 전부 체크 해제 (동일) |
| 핀 | PA11=CAN1_RX, PA12=CAN1_TX | 동일 |

> **Operating Mode = Normal이 핵심.** 버스 상의 다른 노드(F411RE)가 ACK를 보내주지 않으면 TX 프레임이 에러 상태가 된다. 양쪽 노드를 동시에(또는 수초 이내에) 부팅해야 한다.

> **AutoRetransmission = Enable.** Normal 모드에서는 ACK 에러나 비트 에러가 발생할 수 있다. Enable이면 HAL이 자동으로 재전송하므로 일시적 교란을 흡수한다.

- [ ] CAN1 Activated 체크
- [ ] **Operating Mode = Normal**
- [ ] **Automatic Retransmission = Enable**
- [ ] 비트 타이밍: Prescaler=9, BS1=8 TQ, BS2=1 TQ, SJW=1 TQ
- [ ] NVIC: 모든 CAN1 인터럽트 체크 해제 (폴링)
- [ ] Pinout: PA11=CAN1_RX, PA12=CAN1_TX

### Step 3-4: Project Manager 및 코드 생성

- [ ] Toolchain: STM32CubeIDE
- [ ] Generate peripheral init as .c/.h pair: 체크
- [ ] **Generate Code → 빌드 → 에러 없이 .elf 생성**
- [ ] 플래시 → LED 1Hz 점멸, UART "alive" 동작 (CAN 코드 추가 전 known-good 확인)

> 이 시점에서 Normal 모드 CAN1이 활성화되어 있지만 USER CODE가 없으므로 `HAL_CAN_Start`가 호출되지 않았다. 버스에 아무것도 보내지 않는 상태라 에러가 발생하지 않는다.

### Step 3-5: can.c에서 PA11 PULLUP 유지 또는 제거

Phase 1에서 `can.c` `HAL_CAN_MspInit` USER CODE 블록에 PA11 PULLUP을 추가했다. Phase 2에서 SN65HVD230이 RXD 라인을 능동적으로 구동하므로 PULLUP은 이론적으로 필요 없다. 그러나 남겨두어도 동작에 지장 없다.

- Phase 1 can.c의 USER CODE를 그대로 복사하거나, 새 프로젝트 can.c에 동일하게 추가한다.

```c
/* can.c — HAL_CAN_MspInit USER CODE BEGIN 1 */
/* PA11 (CAN_RX) Pull-up: SN65HVD230이 구동하므로 엄밀히 불필요하나 남겨두어도 무해 */
GPIO_InitTypeDef rx_fix = {0};
rx_fix.Pin  = GPIO_PIN_11;
rx_fix.Mode = GPIO_MODE_AF_PP;
rx_fix.Pull = GPIO_PULLUP;
rx_fix.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
rx_fix.Alternate = GPIO_AF9_CAN1;
HAL_GPIO_Init(GPIOA, &rx_fix);
/* can.c — HAL_CAN_MspInit USER CODE END 1 */
```

- [ ] can.c USER CODE에 PA11 PULLUP 코드 배치 (Phase 1과 동일)

### Step 3-6: main.c USER CODE 작성

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
uint32_t self_tx_count = 0;   /* 내가 보낸 프레임 수 (0x010 기준) */
uint32_t peer_rx_count = 0;   /* 상대(F411RE)에서 받은 0x011 프레임 수 */
static uint8_t sawtooth = 0;
/* USER CODE END PV */
```

```c
/* USER CODE BEGIN 2 */
/* --- CAN 필터: 모든 ID 통과 (catch-all) --- */
CAN_FilterTypeDef filter = {0};
filter.FilterBank           = 0;
filter.FilterMode           = CAN_FILTERMODE_IDMASK;
filter.FilterScale          = CAN_FILTERSCALE_32BIT;
filter.FilterIdHigh         = 0x0000;
filter.FilterIdLow          = 0x0000;
filter.FilterMaskIdHigh     = 0x0000;   /* 마스크 0 = 모든 ID 통과 */
filter.FilterMaskIdLow      = 0x0000;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterActivation     = ENABLE;
filter.SlaveStartFilterBank = 14;
HAL_CAN_ConfigFilter(&hcan1, &filter);

HAL_CAN_Start(&hcan1);

/* CAN Start 결과 UART 출력 */
char boot_msg[64];
int boot_len = snprintf(boot_msg, sizeof(boot_msg),
    "[F446RE] CAN start: state=%lu err=0x%lX\r\n",
    (uint32_t)HAL_CAN_GetState(&hcan1),
    HAL_CAN_GetError(&hcan1));
HAL_UART_Transmit(&huart2, (uint8_t*)boot_msg, boot_len, 100);

uint32_t last_hb_tick    = 0;
uint32_t last_state_tick = 0;
uint8_t  led_state       = 0;
/* USER CODE END 2 */
```

```c
/* USER CODE BEGIN WHILE */
while (1)
{
    uint32_t now = HAL_GetTick();

    /* ── 하트비트 TX (0x010, 100ms) ─────────────────────────────── */
    if (now - last_hb_tick >= 100) {
        last_hb_tick = now;

        led_state = !led_state;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

        CAN_TxHeaderTypeDef txh = {0};
        txh.StdId = 0x010;
        txh.IDE   = CAN_ID_STD;
        txh.RTR   = CAN_RTR_DATA;
        txh.DLC   = 8;

        uint8_t tx_data[8] = {0};
        memcpy(tx_data, &self_tx_count, 4);  /* bytes 0-3: counter */
        tx_data[4] = 0x00;                   /* flags */
        uint16_t uptime_s = (uint16_t)(now / 1000);
        tx_data[5] = uptime_s & 0xFF;
        tx_data[6] = (uptime_s >> 8) & 0xFF;
        tx_data[7] = 0x00;

        uint32_t mailbox;
        if (HAL_CAN_AddTxMessage(&hcan1, &txh, tx_data, &mailbox) == HAL_OK) {
            self_tx_count++;
        }

        /* UART 상태 출력 */
        char msg[96];
        int len = snprintf(msg, sizeof(msg),
            "[F446RE] self_tx=%lu peer_rx=%lu err=0x%lX t=%lums\r\n",
            self_tx_count, peer_rx_count,
            HAL_CAN_GetError(&hcan1), now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }

    /* ── 상태 데이터 TX (0x100, 50ms) ───────────────────────────── */
    if (now - last_state_tick >= 50) {
        last_state_tick = now;

        CAN_TxHeaderTypeDef txh = {0};
        txh.StdId = 0x100;
        txh.IDE   = CAN_ID_STD;
        txh.RTR   = CAN_RTR_DATA;
        txh.DLC   = 8;

        uint8_t tx_data[8];
        memset(tx_data, sawtooth, 8);
        sawtooth++;

        uint32_t mailbox;
        HAL_CAN_AddTxMessage(&hcan1, &txh, tx_data, &mailbox);
    }

    /* ── RX 폴링 ─────────────────────────────────────────────────── */
    while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
        CAN_RxHeaderTypeDef rxh;
        uint8_t rx_data[8];
        if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxh, rx_data) == HAL_OK) {
            if (rxh.StdId == 0x011) {   /* F411RE 하트비트 */
                peer_rx_count++;
            }
            /* 0x200 (F411RE 센서)도 수신하지만 별도 카운트 생략 */
        }
    }
/* USER CODE END WHILE */
```

체크리스트:
- [ ] 전역 변수(`self_tx_count`, `peer_rx_count`, `sawtooth`) 선언
- [ ] USER CODE 2: 필터 설정 + `HAL_CAN_Start` + 부트 메시지 출력
- [ ] WHILE: 100ms 하트비트 TX(0x010) + 50ms 상태 TX(0x100) + RX 폴링(0x011 카운트)
- [ ] **빌드 → 에러/경고 없음**

---

## Step 4 — F411RE Phase 2 펌웨어

Phase 1 프로젝트를 수정하지 않는다. 새 프로젝트를 `firmware/f411re_node/phase2_normal/`에 생성한다.

### Step 4-1: 새 프로젝트 생성

Phase 1과 동일한 CubeMX 설정 (RCC, 클럭, USART2, SPI2, PB6=MCP_CS). **변경 사항 없음.** F411RE는 MCP2515를 SPI로 제어하므로 CubeMX 단계에서 CAN 관련 변경이 없다.

- [ ] STM32CubeIDE → New → STM32 Project → NUCLEO-F411RE
- [ ] Project Name: `phase2_normal`, Location: `firmware/f411re_node/phase2_normal/`
- [ ] Phase 1과 동일: RCC=BYPASS, SYSCLK=100MHz, APB1=50MHz, SPI2 Full-Duplex Master, PB6=MCP_CS(초기 High)
- [ ] **빌드 → 에러 없음 → 플래시 → LED 1Hz + UART alive 확인** (known-good 상태)

### Step 4-2: main.c USER CODE 작성

MCP2515 헬퍼 함수는 Phase 1과 동일. **`mcp2515_init_normal`이 새로 추가되고, `mcp2515_init_loopback`은 사용하지 않는다.**

```c
/* USER CODE BEGIN PD */
/* MCP2515 SPI 명령어 (Phase 1과 동일) */
#define MCP_RESET        0xC0
#define MCP_READ         0x03
#define MCP_WRITE        0x02
#define MCP_BIT_MODIFY   0x05
#define MCP_RTS_TX0      0x81

/* MCP2515 레지스터 주소 (Phase 1과 동일) */
#define MCP_CNF3         0x28
#define MCP_CNF2         0x29
#define MCP_CNF1         0x2A
#define MCP_CANINTF      0x2C
#define MCP_EFLG         0x2D
#define MCP_CANCTRL      0x0F
#define MCP_CANSTAT      0x0E
#define MCP_TXB0CTRL     0x30
#define MCP_TXB0SIDH     0x31
#define MCP_TXB0SIDL     0x32
#define MCP_TXB0DLC      0x35
#define MCP_TXB0D0       0x36
#define MCP_RXB0CTRL     0x60
#define MCP_RXB0SIDH     0x61
#define MCP_RXB0SIDL     0x62
#define MCP_RXB0DLC      0x65
#define MCP_RXB0D0       0x66

/* CANCTRL 모드 비트[7:5] */
#define MCP_MODE_NORMAL   0x00   /* ← Phase 2에서 사용 */
#define MCP_MODE_LOOPBACK 0x40
#define MCP_MODE_CONFIG   0x80

#define MCP_CS_LOW()   HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET)
#define MCP_CS_HIGH()  HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET)
/* USER CODE END PD */

/* USER CODE BEGIN PV */
uint32_t self_tx_count = 0;   /* 내가 보낸 프레임 수 (0x011 기준) */
uint32_t peer_rx_count = 0;   /* 상대(F446RE)에서 받은 0x010 프레임 수 */
static uint8_t sawtooth = 0;
/* USER CODE END PV */
```

헬퍼 함수 (Phase 1 함수 그대로 + **`mcp2515_init_normal` 추가**):

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

/* Phase 2: Normal 모드 초기화 (Phase 1 loopback 대체) */
static void mcp2515_init_normal(void) {
    mcp2515_reset();
    mcp2515_write(MCP_CNF1, 0x00);       /* BRP=0, SJW=1 (Phase 1과 동일) */
    mcp2515_write(MCP_CNF2, 0x91);       /* PhSeg1=3TQ, PropSeg=2TQ */
    mcp2515_write(MCP_CNF3, 0x01);       /* PhSeg2=2TQ → 500 kbps @ 8MHz */
    mcp2515_write(MCP_RXB0CTRL, 0x60);   /* RXB0: 필터 바이패스 (모든 프레임 수신) */
    mcp2515_write(MCP_CANCTRL, MCP_MODE_NORMAL); /* ← 핵심 변경: 0x00 = Normal */
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

메인 초기화 및 루프:

```c
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

uint32_t last_hb_tick    = 0;
uint32_t last_state_tick = 0;
uint8_t  led_state       = 0;
/* USER CODE END 2 */
```

```c
/* USER CODE BEGIN WHILE */
while (1)
{
    uint32_t now = HAL_GetTick();

    /* ── 하트비트 TX (0x011, 100ms) ─────────────────────────────── */
    if (now - last_hb_tick >= 100) {
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

        /* TX 완료 대기 (최대 10ms): Normal 모드는 버스 점유 시간 필요 */
        uint32_t t = HAL_GetTick();
        while ((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 10)) {}

        /* TX 성공 여부: TXERR 비트(0x10) 없으면 성공 */
        if (!(mcp2515_read(MCP_TXB0CTRL) & 0x10)) {
            self_tx_count++;
        }

        /* UART 상태 출력 */
        uint8_t eflg = mcp2515_read(MCP_EFLG);
        char msg[96];
        int len = snprintf(msg, sizeof(msg),
            "[F411RE] self_tx=%lu peer_rx=%lu eflg=0x%02X t=%lums\r\n",
            self_tx_count, peer_rx_count, eflg, now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }

    /* ── 센서 데이터 TX (0x200, 50ms) ───────────────────────────── */
    if (now - last_state_tick >= 50) {
        last_state_tick = now;

        uint8_t tx_data[8];
        memset(tx_data, sawtooth, 8);
        sawtooth++;

        mcp2515_send_frame(0x200, tx_data, 8);

        uint32_t t = HAL_GetTick();
        while ((mcp2515_read(MCP_TXB0CTRL) & 0x08) && (HAL_GetTick() - t < 10)) {}
    }

    /* ── RX 폴링 ─────────────────────────────────────────────────── */
    if (mcp2515_read(MCP_CANINTF) & 0x01) {    /* RX0IF */
        uint8_t rx_sidh = mcp2515_read(MCP_RXB0SIDH);
        uint8_t rx_sidl = mcp2515_read(MCP_RXB0SIDL);
        uint16_t rx_id  = ((uint16_t)rx_sidh << 3) | (rx_sidl >> 5);

        uint8_t rx_len = mcp2515_read(MCP_RXB0DLC) & 0x0F;
        uint8_t rx_data[8];
        for (uint8_t i = 0; i < rx_len && i < 8; i++) {
            rx_data[i] = mcp2515_read(MCP_RXB0D0 + i);
        }
        mcp2515_bit_modify(MCP_CANINTF, 0x01, 0x00);   /* RX0IF 클리어 */

        if (rx_id == 0x010) {    /* F446RE 하트비트 */
            peer_rx_count++;
        }
    }
/* USER CODE END WHILE */
```

체크리스트:
- [ ] 전역 변수(`self_tx_count`, `peer_rx_count`, `sawtooth`) 선언
- [ ] `mcp2515_init_normal()` 헬퍼 함수 추가 (CANCTRL = `0x00`)
- [ ] 부팅 메시지: `CANSTAT=0x00`이면 "OK" (Normal 모드 확인)
- [ ] WHILE: 100ms 하트비트 TX(0x011) + 50ms 센서 TX(0x200) + RX 폴링(0x010 카운트)
- [ ] **빌드 → 에러/경고 없음**

> **`[F411RE] init OK (CANSTAT=0x00)`** 출력이 핵심. 0x00이면 Normal 모드 진입 성공.
> 0x80 (Config 유지)이 뜨면 CANCTRL 쓰기가 실패한 것 — SPI 배선 재확인.

---

## Step 5 — 순차 검증 (단방향 먼저)

**양쪽을 동시에 켜기 전에, 한쪽 TX 동작을 먼저 분리 검증한다.**

> Normal 모드에서는 ACK가 없으면 에러가 쌓인다. F446RE만 켜고 F411RE가 없으면 F446RE가 ACK 에러를 누적해 에러 패시브 또는 버스 오프로 전환된다. 따라서 아래 순서를 따른다.

### Step 5-1: 양쪽 노드 동시 부팅

- [ ] F446RE에 `phase2_normal` 플래시 (전원 차단 상태에서)
- [ ] F411RE에 `phase2_normal` 플래시 (전원 차단 상태에서)
- [ ] **두 보드를 동시에 USB 연결** (또는 수초 이내에 순서대로 연결)
- [ ] Tera Term 두 창에서 각 UART 출력 확인

### Step 5-2: 부팅 메시지 확인

F446RE Tera Term:
```
[F446RE] CAN start: state=2 err=0x0
```
- `state=2` = HAL_CAN_STATE_READY → 정상
- `err=0x0` → 에러 없음

F411RE Tera Term:
```
[F411RE] init OK (CANSTAT=0x00)
```
- `CANSTAT=0x00` → Normal 모드 진입 성공

- [ ] F446RE: `state=2 err=0x0` 확인
- [ ] F411RE: `CANSTAT=0x00` (init OK) 확인

---

## Step 6 — 양방향 동작 검증

양쪽 노드가 부팅되면, 약 1~2초 안에 서로의 프레임을 수신하기 시작해야 한다.

### 예상 UART 출력

**F446RE 창:**
```
[F446RE] self_tx=1  peer_rx=0  err=0x0 t=103ms
[F446RE] self_tx=2  peer_rx=1  err=0x0 t=203ms
[F446RE] self_tx=3  peer_rx=2  err=0x0 t=303ms
[F446RE] self_tx=10 peer_rx=9  err=0x0 t=1003ms
...
```

**F411RE 창:**
```
[F411RE] self_tx=1  peer_rx=0  eflg=0x00 t=103ms
[F411RE] self_tx=2  peer_rx=1  eflg=0x00 t=203ms
[F411RE] self_tx=3  peer_rx=2  eflg=0x00 t=303ms
...
```

### 검증 항목

- [ ] F446RE `peer_rx`가 `self_tx`보다 최대 1 뒤처지며 꾸준히 증가 (초당 약 10 증가)
- [ ] F411RE `peer_rx`가 `self_tx`보다 최대 1 뒤처지며 꾸준히 증가
- [ ] **F446RE `err=0x0`** — ACK 에러, 비트 에러 없음
- [ ] **F411RE `eflg=0x00`** — 에러 플래그 없음 (TXWARN, RXWARN, TXEP, RXEP, TXBO 모두 0)
- [ ] 두 창 모두 `self_tx`가 단조 증가 (재시작 없음)

> **`peer_rx`가 0에 머문다:** 물리 레이어 문제. Step 7 실패 모드 표 참조.
>
> **`err` 또는 `eflg`에 값이 있다:** 에러 카운터가 쌓이는 중. 배선 연속성과 GND 공통 연결 재확인.

---

## Step 7 — 케이블 분리/재연결 테스트

통신이 안정화된 후, 버스 내성을 간단히 확인한다.

- [ ] 안정 통신 중 CAN_H 또는 CAN_L 케이블 한 가닥을 **5초간 분리**.
  - 양쪽 `peer_rx`가 멈추고, F446RE `err`나 F411RE `eflg`에 값이 생길 수 있음.
- [ ] 케이블 **재연결**. 10초 이내에 `peer_rx`가 다시 증가하기 시작하는지 확인.
- [ ] `self_tx`는 케이블 분리/재연결과 무관하게 계속 증가 (송신 시도 지속).

> 이 테스트에서 완전한 자동 복구를 요구하지 않는다 (Bus-Off 복구는 Phase 5 범위). 케이블 재연결 후 재부팅 없이 통신이 재개되면 Pass.

---

## Step 8 — 30초 이상 안정성 관찰

- [ ] 분리/재연결 테스트 후 정상 연결 상태에서 30초 이상 관찰
- [ ] F446RE `peer_rx`가 단조 증가하고 `err=0x0` 유지
- [ ] F411RE `peer_rx`가 단조 증가하고 `eflg=0x00` 유지
- [ ] 두 보드 모두 비정상적으로 따뜻해지지 않음
- [ ] UART 메시지가 중단되거나 글자가 깨지지 않음

---

## 주의해야 할 실패 모드

| 증상 | 예상 원인 | 조치 |
|------|-----------|------|
| F446RE `peer_rx=0` 유지, `err=0x0` | F411RE가 아직 부팅되지 않음 또는 Normal 모드 아님 | F411RE init OK(CANSTAT=0x00) 확인. 두 노드 동시 부팅 재시도 |
| F446RE `err≠0x0` (ACK 에러 포함) | F411RE가 ACK를 보내지 않음: F411RE 미부팅, 또는 배선 단선 | CANH/CANL 연속성 확인. GND 공통 연결 확인 |
| F411RE `eflg=0x18` (TXEP+RXEP) | 에러 패시브 진입: ACK 미수신 반복 | F446RE가 정상 동작 중인지 확인. GND 공통 연결 필수 |
| F411RE `eflg=0x20` (TXBO) | 버스 오프: 에러 카운터 256 초과 | F411RE 재부팅. 원인 해결 후 재시도 |
| 양쪽 `peer_rx=0`, 에러도 없음 | 모드 설정 오류 (F446RE=Loopback 유지 가능성) | F446RE CubeMX CAN1 Operating Mode = Normal 재확인 |
| `peer_rx`가 간헐적으로 멈춤 | GND 공통 연결 접촉 불량 또는 종단 저항 누락 | GND 배선 재확인, CANH-CANL 60Ω 측정 |
| UART 출력이 극히 느려짐 | HAL_UART_Transmit 100ms 타임아웃이 루프 주기보다 길어짐 | UART 타임아웃을 50ms 이하로 줄이거나 DMA 사용 검토 |
| SN65HVD230 모듈이 즉시 따뜻해짐 | VCC를 5V에 연결함 (SN65HVD230은 3.3V 전용) | **즉시 전원 차단.** F446RE 3.3V 헤더에서 공급 |
| F411RE `init FAIL (CANSTAT=0x80)` | mcp2515_init_normal에서 CANCTRL 쓰기 실패 (SPI 문제) | SPI 배선 연속성 재확인. CS 초기값 High 확인 |

---

## 사인오프

아래 모든 항목이 충족될 때 Phase 2가 완료된다:

- [ ] 양쪽 노드 부팅 후 `peer_rx`가 10초 이내에 증가하기 시작함
- [ ] 30초 이상 F446RE `err=0x0`, F411RE `eflg=0x00` 유지
- [ ] 케이블 분리/재연결 후 재부팅 없이 통신 재개됨
- [ ] 스크린샷 또는 로그를 `docs/assets/captures/` 저장

커밋:
```
git add firmware/f446re_node/phase2_normal/
git add firmware/f411re_node/phase2_normal/
git commit -m "phase2: F446RE<->F411RE CAN bus verified, bidirectional 500kbps"
```

**이 커밋이 존재하기 전까지 Phase 3(RPi5 게이트웨이)을 시작할 수 없다.**

---

## Phase 3로 넘어가기 전 체크포인트

Phase 3는 RPi5 노드 추가다. Phase 2 완료 직후 아래를 미리 점검한다:

- [ ] RPi5(#29) 전원 공급 및 OS 부팅 확인
- [ ] MCP2515+TJA1050 모듈 (#34) 두 번째 세트 확보 (RPi5용 SPI 연결)
- [ ] TJA1050 5V 출력 GPIO 손상 위험 검토: RPi5 GPIO는 3.3V 내성. 레벨 컨버터 또는 SN65HVD230(3.3V) 대체 검토 필요
- [ ] 3노드 버스에서 종단 저항 재배치 계획 (버스 양 끝단 2개 유지, 중간 노드는 종단 없음)
