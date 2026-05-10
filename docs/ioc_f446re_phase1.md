# STM32CubeMX .ioc Configuration — F446RE Phase 1 (phase1_loopback)

Phase 1 목표: bxCAN 페리퍼럴을 **내부 루프백 모드**로 동작시켜, TX 메일박스에서 보낸 프레임이 RX FIFO에 그대로 도착하는지 검증한다.
트랜시버, 외부 버스, 다른 노드 없음. MCU 내부에서 완결되는 테스트.

---

## 이 단계의 의의

Phase 1은 **bxCAN 페리퍼럴 자체의 설정이 올바른가**만 검증한다. 구체적으로:

- APB1 클럭(45 MHz)이 CAN1에 정확히 도달하는가
- 비트 타이밍 계산이 500 kbps로 떨어지는가
- 메시지 필터 설정이 RX FIFO를 통과시키는가
- TX 메일박스 → RX FIFO 흐름 자체가 동작하는가

루프백 모드에서는 CAN_TX/CAN_RX 핀이 사용되지 않고, ACK가 자체 생성되므로 트랜시버나 종단 저항 없이 검증 가능하다. Phase 2에서 외부 트랜시버 연결 시, 변수가 늘어나는 것은 "물리 레이어"뿐이다 — 페리퍼럴 설정 변수는 Phase 1에서 이미 잡혔다.

---

## 1. 새 프로젝트 생성

STM32CubeIDE → File → New → STM32 Project

| 항목 | 값 |
|------|----|
| Board Selector 탭 | `NUCLEO-F446RE` 검색 후 선택 |
| Project Name | `phase1_loopback` |
| Project Location | `firmware/f446re_node/phase1_loopback/` |
| Targeted Language | C |
| Targeted Binary Type | Executable |
| Targeted Project Type | STM32Cube |

> Board Selector 사용 후 "Initialize all peripherals with their default Mode?" 질문에는 **`No`** 선택.
> `Yes`를 누르면 ETH, USB OTG 등 Phase 1에서 쓰지 않는 페리퍼럴까지 자동 활성화되어, 우리가 켠 것과 자동으로 켜진 것의 구분이 흐려진다.
> `No`를 선택해도 LD2(PA5)와 USART2는 Board Selector가 자동으로 잡아 준다.

---

## 2. RCC, SYS, GPIO, USART2 — Phase 0와 동일

이 항목들은 [`ioc_f446re_phase0.md`](ioc_f446re_phase0.md)의 Step 3, 4, 5, 6을 그대로 따른다.

요약:

| 페리퍼럴 | 설정 |
|----------|------|
| RCC | HSE = BYPASS Clock Source |
| Clock Config | SYSCLK = 180 MHz, **APB1 = 45 MHz**, APB2 = 90 MHz |
| SYS | Debug = Serial Wire, Timebase = SysTick |
| GPIO PA5 | LD2, GPIO_Output Push Pull, Low, Low speed, No pull |
| USART2 | Asynchronous, 115200 8N1, RX/TX 모두 |

**APB1 = 45 MHz를 반드시 확인할 것.** 이 값이 다음 항목의 비트 타이밍 계산의 입력이다. APB1이 45 MHz가 아니면 아래 Prescaler/BS1/BS2 값으로 500 kbps가 나오지 않는다.

---

## 3. 비트 타이밍 계산 (CAN1 설정 전 사전 작업)

bxCAN 비트 타이밍은 4개 값으로 결정된다:

```
1 비트 시간 = (1 + BS1 + BS2) × Time Quantum
Time Quantum = Prescaler / CAN_Clock
비트레이트 = CAN_Clock / (Prescaler × (1 + BS1 + BS2))
```

**입력값:**

```
CAN_Clock = APB1 = 45 MHz
목표 비트레이트 = 500 kbps
목표 샘플 포인트 = 87.5%  (자동차/CiA 권장)
```

**Step 3-1. 분주 계산**

```
45,000,000 / 500,000 = 90
→ Prescaler × (1 + BS1 + BS2) = 90
```

**Step 3-2. Total TQ 선택**

90을 분해할 때, `1 + BS1 + BS2`(= Total TQ)가 8~25 범위에 들어가는 조합을 선호한다 (CiA 권장).

| Prescaler | Total TQ | 비고 |
|-----------|----------|------|
| 3 | 30 | TQ가 너무 많음 |
| **5** | **18** | 권장 — 87.5%에 잘 맞음 |
| 6 | 15 | 가능 |
| 9 | 10 | TQ가 적음, 지터 여유 부족 |

**Total TQ = 18, Prescaler = 5 선택.**

**Step 3-3. BS1, BS2 분배**

샘플 포인트 = `(1 + BS1) / (1 + BS1 + BS2)` 가 87.5%에 가깝도록:

```
(1 + BS1) / 18 ≈ 0.875
→ 1 + BS1 ≈ 15.75
→ BS1 = 15  (반올림)
→ BS2 = 18 - 1 - 15 = 2

실제 샘플 포인트 = 16 / 18 = 88.9%  ← 87.5%에 충분히 근접
```

**Step 3-4. 검증**

```
45,000,000 / (5 × (1 + 15 + 2)) = 45,000,000 / 90 = 500,000 bps  ✓
```

**최종 값:**

| 파라미터 | 값 |
|----------|-----|
| Prescaler (for Time Quantum) | 5 |
| Time Quanta in Bit Segment 1 | 15 |
| Time Quanta in Bit Segment 2 | 2 |
| ReSynchronization Jump Width | 1 |

> 이 값은 Phase 2(F411RE 합류)와 Phase 3(RPi MCP2515 합류)에서도 동일하게 적용된다. 한 노드라도 비트 타이밍이 어긋나면 ACK 에러가 나고 송신 노드는 버스 오프로 진입한다. 이번 단계에서 정확히 잡아 둘 것.

---

## 4. CAN1 — 페리퍼럴 활성화

`Connectivity → CAN1`

**Mode 영역:**

| 항목 | 설정 |
|------|------|
| Activated | ✅ 체크 |
| Master | (자동, F446RE의 CAN1은 Master) |

**Parameter Settings 탭 — Bit Timings Parameters:**

| 항목 | 값 |
|------|-----|
| Prescaler (for Time Quantum) | `5` |
| Time Quanta in Bit Segment 1 | `15 Times` |
| Time Quanta in Bit Segment 2 | `2 Times` |
| ReSynchronization Jump Width | `1 Time` |

**Parameter Settings 탭 — Basic Parameters:**

| 항목 | 값 | 이유 |
|------|-----|------|
| **Operating Mode** | **`Loopback`** | Phase 1의 핵심. Normal 모드에서는 ACK가 안 와서 첫 프레임이 영원히 펜딩된다. |
| Time Triggered Communication Mode | Disable | TTCAN 미사용 |
| Automatic Bus-Off Management | Enable | 버스 오프 자동 복구 (Phase 5에서 본격 검증, 여기서는 켜 두기만 함) |
| Automatic Wake-Up Mode | Disable | Sleep 모드 미사용 |
| Automatic Retransmission | Enable | 일반적인 CAN 동작 |
| Receive FIFO Locked Mode | Disable | FIFO 가득 찰 시 새 프레임으로 덮어쓰기 |
| Transmit FIFO Priority | Disable | ID 우선순위 (FIFO 순서가 아닌) |

**NVIC Settings 탭:**

| 항목 | 값 |
|------|-----|
| 모든 CAN1 인터럽트 | ❌ **전부 체크 해제** |

폴링으로 진행한다. 인터럽트 핸들러 설정과 bxCAN 자체 설정을 동시에 디버깅하지 않기 위함. Phase 4(스케줄링)에서 인터럽트로 전환.

---

## 5. 핀 할당 확인

CAN1 활성화 시 CubeMX가 핀을 자동으로 잡는다. Pinout view에서 확인:

| 핀 | Signal |
|----|--------|
| **PA11** | CAN1_RX |
| **PA12** | CAN1_TX |

만약 PB8/PB9로 잡혀 있다면 **PA11/PA12로 변경**한다:

1. PB8 우클릭 → Reset
2. PB9 우클릭 → Reset
3. PA11 클릭 → CAN1_RX 선택
4. PA12 클릭 → CAN1_TX 선택

루프백 모드에서는 핀이 실제로 사용되지 않으므로 동작에는 영향이 없다. 하지만 Phase 2에서 트랜시버를 연결할 때 PA11/PA12가 NUCLEO 보드의 외부 헤더로 더 깔끔하게 빠지므로, 지금 잡아 두는 것이 다음 단계의 배선 변수를 줄인다.

---

## 6. 활성화하지 말아야 할 것

| 항목 | 이유 |
|------|------|
| CAN2 | F446RE는 CAN2도 있지만 단일 CAN 버스 프로젝트에서 불필요. Phase 1~5 어디서도 쓰지 않는다. |
| FreeRTOS | Phase 1은 단일 루프 폴링. 스케줄러 불필요. |
| DMA | 폴링 송수신으로 충분. |
| 추가 타이머 | 주기 송신은 `HAL_GetTick()` 비교로 충분. |
| SPI / I2C | F446RE는 MCP2515를 쓰지 않는다 (RPi 노드 전용). |
| TIM 인터럽트 | 위와 동일. |

---

## 7. Project Manager 탭 — Phase 0와 동일

| 항목 | 값 |
|------|-----|
| Project Name | phase1_loopback |
| Toolchain / IDE | **STM32CubeIDE** |
| Minimum Heap Size | 0x200 |
| Minimum Stack Size | 0x400 |
| Generate peripheral init. as a pair of .c/.h files | ✅ **체크** |

`Code Generator` 탭:

| 항목 | 값 |
|------|-----|
| Copy only necessary library files | 선택 |
| Generate peripheral init. as pair | ✅ |
| Keep user code when re-generating | ✅ 기본값 |

---

## 8. 코드 생성 후 확인 사항

Generate Code 직후 다음을 확인:

1. `Core/Inc/can.h`, `Core/Src/can.c` 생성됨
2. `main.c`의 `MX_CAN1_Init()` 호출 추가됨
3. `SystemClock_Config()`에서 APB1 분주비 `RCC_HCLK_DIV4` 유지 확인
4. **빌드 (Ctrl+B) → 에러 없이 `.elf` 생성** ← Step 9 직전 베이스라인
5. 플래시 → Phase 0와 동일한 동작 확인 (LED 1Hz 점멸 + UART "alive" 메시지)

5번이 중요하다. 아직 USER CODE에서 CAN을 호출하지 않았으므로, CAN1만 켰을 때 Phase 0 동작이 그대로 유지되어야 한다. 깨졌다면 클럭이나 핀 충돌이므로 다음 단계로 넘어가지 않는다.

---

## 9. USER CODE 작성 가이드 (설정 후 작성)

`.ioc` 설정 완료 후 USER CODE 블록에 작성할 핵심 골격. 실제 코드는 `app_can_loop.c/h`에 분리하는 것을 권장 ([workflow.md](../workflow.md) 참조).

**필요한 것 4가지:**

1. **필터 설정** — 필터를 설정하지 않으면 RX FIFO에 어떤 프레임도 들어가지 않는다.
2. **CAN Start 호출** — `HAL_CAN_Start()` 호출 전까지 페리퍼럴은 INIT 모드에 머문다.
3. **TX 송신** — `HAL_CAN_AddTxMessage()`로 메일박스에 적재.
4. **RX 폴링** — `HAL_CAN_GetRxFifoFillLevel()`로 FIFO 점검 후 `HAL_CAN_GetRxMessage()`.

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
uint32_t tx_count = 0;
uint32_t rx_count = 0;
/* USER CODE END PV */

/* USER CODE BEGIN 2 */
/* --- CAN 필터: 모든 ID 통과 --- */
CAN_FilterTypeDef filter = {0};
filter.FilterBank          = 0;
filter.FilterMode          = CAN_FILTERMODE_IDMASK;
filter.FilterScale         = CAN_FILTERSCALE_32BIT;
filter.FilterIdHigh        = 0x0000;
filter.FilterIdLow         = 0x0000;
filter.FilterMaskIdHigh    = 0x0000;   /* 마스크 0 = 모든 ID 통과 */
filter.FilterMaskIdLow     = 0x0000;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterActivation    = ENABLE;
filter.SlaveStartFilterBank = 14;
HAL_CAN_ConfigFilter(&hcan1, &filter);

/* --- CAN 시작 --- */
HAL_CAN_Start(&hcan1);

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

        /* --- TX --- */
        CAN_TxHeaderTypeDef tx_header = {0};
        tx_header.StdId = 0x123;
        tx_header.IDE   = CAN_ID_STD;
        tx_header.RTR   = CAN_RTR_DATA;
        tx_header.DLC   = 8;
        uint8_t tx_data[8];
        memcpy(tx_data, &tx_count, 4);   /* 카운터를 페이로드에 실음 */
        memset(tx_data + 4, 0xAA, 4);

        uint32_t mailbox;
        if (HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &mailbox) == HAL_OK) {
            tx_count++;
        }

        /* --- RX 폴링 --- */
        if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
            CAN_RxHeaderTypeDef rx_header;
            uint8_t rx_data[8];
            if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
                rx_count++;
            }
        }

        /* --- UART 출력 --- */
        char msg[64];
        int len = snprintf(msg, sizeof(msg),
                           "[F446RE] tx=%lu rx=%lu t=%lums\r\n",
                           tx_count, rx_count, now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }
}
/* USER CODE END WHILE */
```

> `string.h`를 USER CODE BEGIN Includes에 추가할 것 (`memcpy`, `memset`).
> 위 코드는 처음 학습용으로 USER CODE 안에 직접 넣었다. 안정화 후 `app_can_loop.c/h`로 분리하면 .ioc 재생성 시 안전하다.

---

## 요약

```
활성화: Phase 0 + CAN1(Loopback, 500kbps, BS1=15/BS2=2/Pre=5, PA11/PA12)
비활성화: CAN2, FreeRTOS, DMA, 인터럽트, 추가 타이머
검증: 빌드 OK + LED 1Hz + UART에서 tx/rx 카운터가 같이 증가
```

Phase 1 sign-off 기준은 [`phase1_checklist.md`](phase1_checklist.md) 참조.