# STM32CubeMX .ioc Configuration — F411RE Phase 0 (phase0_alive)

Phase 0 목표: LED 1Hz 점멸 + UART로 "alive" 메시지 1초마다 출력.
CAN, FreeRTOS, 타이머 인터럽트 없음. 최소 구성.

> 본 문서는 [`ioc_f446re.md`](ioc_f446re.md)의 F446RE Phase 0 설정을 기반으로 작성되었으며,
> **F411RE와 다른 지점만 강조 표시**한다. 동일한 항목은 짧게 요약하고 F446RE 문서를 참조.

---

## F446RE와의 핵심 차이

본격 설정에 들어가기 전에, F411RE에서 달라지는 지점을 먼저 정리한다:

| 항목 | F446RE | **F411RE** | 영향 |
|------|--------|-----------|------|
| SYSCLK 최대 | 180 MHz | **100 MHz** | 클럭 트리 전체 재계산 |
| APB1 최대 | 45 MHz | **50 MHz** | Phase 1 비트 타이밍 분주 달라짐 |
| APB2 최대 | 90 MHz | **100 MHz** | (Phase 0에선 무관) |
| bxCAN | CAN1 + CAN2 | **CAN1만** | Phase 1·2 영향 없음 (어차피 CAN1만 사용) |
| FDCAN 지원 | ✅ | ❌ | CAN-FD 마이그레이션 시 노드 교체 필요 |
| UART 라벨 | `[F446RE]` | **`[F411RE]`** | Phase 2 시리얼 동시 관찰 시 즉시 구분 |

GPIO, USART2, SWD, NUCLEO 보드 레이아웃은 F446RE와 동일. 즉 클럭과 UART 라벨만 신경 쓰면 된다.

---

## 1. 새 프로젝트 생성

STM32CubeIDE → File → New → STM32 Project

| 항목 | 값 |
|------|----|
| Board Selector 탭 | `NUCLEO-F411RE` 검색 후 선택 |
| Project Name | `phase0_alive` |
| Project Location | `firmware/f411re_node/phase0_alive/` |
| Targeted Language | C |
| Targeted Binary Type | Executable |
| Targeted Project Type | STM32Cube |

> "Initialize all peripherals with their default Mode?" → **`No`** 선택 (F446RE Phase 1에서 학습한 규칙).
> `Yes`를 선택하면 우리가 켠 것과 자동으로 켜진 것의 구분이 흐려진다.
> `No`를 선택해도 LD2(PA5)와 USART2는 Board Selector가 자동으로 잡아 준다.

---

## 2. RCC — 클럭 소스 (F446RE와 동일)

`System Core → RCC`

| 항목 | 설정 | 이유 |
|------|------|------|
| High Speed Clock (HSE) | **BYPASS Clock Source** | NUCLEO-F411RE도 ST-Link MCO(8MHz)를 통해 HSE를 공급. Crystal/Resonator로 설정하면 클럭이 잡히지 않는다. |
| Low Speed Clock (LSE) | Disable | Phase 0에서 RTC 불필요 |

---

## 3. Clock Configuration 탭 — PLL 설정 ⚠ F446RE와 다름

목표: SYSCLK = **100 MHz** (F411RE 최대)

```
HSE = 8 MHz (ST-Link MCO)
  │
  └─→ PLL Source Mux: HSE
        PLLM = 8   → VCO 입력 = 8 / 8 = 1 MHz   (권장 범위 1–2 MHz)
        PLLN = 200 → VCO 출력 = 1 × 200 = 200 MHz
        PLLP = 2   → SYSCLK  = 200 / 2 = 100 MHz
        PLLQ = 4   → (USB용, Phase 0에서 사용 안 하지만 기입)
```

| 버스 | 분주비 | 결과 | 상한 |
|------|--------|------|------|
| AHB  | /1  | 100 MHz | 100 MHz |
| APB1 | /2  | **50 MHz** | 50 MHz ← 반드시 지킬 것 |
| APB2 | /1  | 100 MHz | 100 MHz |

> **F446RE와의 비교:** F446RE는 APB1을 /4로 나눠 45 MHz를 만든다. F411RE는 SYSCLK 자체가 낮아서 /2로도 50 MHz 상한 내에 들어온다. CubeMX가 APB1 위반을 빨간색으로 표시하므로, 다른 분주비를 선택했다면 화면에서 즉시 보인다.
>
> **Phase 1 미리보기:** 이 APB1 = 50 MHz가 Phase 1 비트 타이밍의 입력이 된다. F446RE의 `Prescaler=9, BS1=8, BS2=1` 값을 그대로 가져오면 500 kbps가 아니라 약 555 kbps가 나와서, Phase 2에서 F446RE와 통신할 때 ACK 에러로 직행한다. (자세한 재계산은 문서 끝 "Phase 1 미리보기" 참조.)

**HAL Timebase Source:** SysTick (기본값 유지).

---

## 4. SYS — 디버그 설정 (F446RE와 동일)

`System Core → SYS`

| 항목 | 설정 |
|------|------|
| Debug | Serial Wire (SWD) |
| Timebase Source | SysTick |

---

## 5. GPIO — LED (F446RE와 동일)

NUCLEO Board Selector 사용 시 PA5가 자동으로 잡혀 있다. 확인만:

| 핀 | Label | Mode | Output Level | Speed | Pull |
|----|-------|------|-------------|-------|------|
| PA5 | LD2 | GPIO_Output Push Pull | Low | Low | No pull |

B1 버튼(PC13)은 Phase 0에서 쓰지 않지만 자동으로 잡혀 있어도 무방.

---

## 6. USART2 — "alive" 메시지 출력 (F446RE와 동일)

`Connectivity → USART2`

NUCLEO-F411RE의 ST-Link 가상 COM 포트도 USART2(PA2=TX, PA3=RX)에 연결되어 있다.

| 항목 | 값 |
|------|-----|
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Word Length | 8 Bits (including Parity) |
| Parity | None |
| Stop Bits | 1 |
| Data Direction | Receive and Transmit |
| Over Sampling | 16 Samples |
| Hardware Flow Control | Disable |

DMA, 인터럽트 모두 비활성화. 폴링으로 충분.

핀 확인:

| 핀 | Signal | 방향 |
|----|--------|------|
| PA2 | USART2_TX | MCU → ST-Link → PC |
| PA3 | USART2_RX | PC → ST-Link → MCU |

> **APB1=50MHz에서 115200 보레이트 검산:** USART2는 APB1 클럭을 사용. 50,000,000 / (16 × 115200) ≈ 27.13 → USARTDIV 정수부 27, 소수부 ≈ 0.13 → 실제 보레이트 ≈ 115,740 bps → 오차 약 0.47%. 허용 범위(보통 ±2%) 내. CubeMX가 자동 계산하므로 별도 작업 불필요.

---

## 7. 활성화하지 말아야 할 것

| 항목 | 이유 |
|------|------|
| CAN1 | Phase 1부터. 지금 켜면 Phase 1에서 추가한 것인지 Phase 0에서 실수로 켠 것인지 구분이 안 된다. |
| FreeRTOS | Phase 0에 스케줄러 불필요. |
| DMA | 폴링 UART로 충분. |
| 추가 타이머(TIM2 등) | 1Hz 점멸은 `HAL_GetTick()` 기반 비교로 구현. |
| SPI / I2C | 사용 안 함. |

---

## 8. Project Manager 탭 (F446RE와 동일)

| 항목 | 값 |
|------|-----|
| Project Name | phase0_alive |
| Project Location | `firmware/f411re_node/phase0_alive/` |
| Toolchain / IDE | **STM32CubeIDE** |
| Minimum Heap Size | 0x200 |
| Minimum Stack Size | 0x400 |
| Generate peripheral init. as a pair of .c/.h files | **체크** |

`Code Generator` 탭:

| 항목 | 값 |
|------|-----|
| Copy only necessary library files | 선택 |
| Generate peripheral init. as pair | ✅ |
| Keep user code when re-generating | ✅ 기본값 |

---

## 9. 코드 생성 후 확인 사항

1. `Core/Src/main.c` → `MX_USART2_UART_Init()`, `MX_GPIO_Init()` 호출 확인.
2. `SystemClock_Config()` → `HAL_RCC_ClockConfig()`에서 APB1 분주비 **`RCC_HCLK_DIV2`** 확인 (F446RE의 DIV4와 다름).
3. `SystemClock_Config()` → PLL 설정에서 PLLM=8, PLLN=200, PLLP=RCC_PLLP_DIV2 확인.
4. 빌드(Ctrl+B) → 에러 없이 `.elf` 생성 확인.
5. 플래시 → LED 점멸 없음이 정상 (아직 USER CODE 없음).

---

## 10. USER CODE 작성 가이드

F446RE와 동일하되 **출력 문자열만 `[F411RE]`로 변경**:

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
uint32_t last_tick = 0;
uint8_t led_state = 0;
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    uint32_t now = HAL_GetTick();
    if (now - last_tick >= 500) {   // 500ms마다 토글 → 1Hz 점멸
        last_tick = now;
        led_state = !led_state;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, led_state);

        char msg[48];
        int len = snprintf(msg, sizeof(msg), "[F411RE] alive, t=%lums\r\n", now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }
}
/* USER CODE END WHILE */
```

> 라벨이 `[F411RE]`인 이유: Phase 2에서 양쪽 노드 시리얼을 동시에 띄울 때 어느 보드의 출력인지 한 글자도 의심하지 않고 구분되어야 한다. F446RE는 `[F446RE]`, F411RE는 `[F411RE]`로 고정.

---

## 요약

```
활성화: HSE(BYPASS) → PLL 100MHz → SysTick → PA5(LED) → USART2(115200)
비활성화: CAN, FreeRTOS, DMA, SPI, 추가 타이머
검증: 빌드 성공 + LED 1Hz + 터미널에서 "[F411RE] alive, t=Xms" 1초마다 출력
```

Phase 0 sign-off 기준은 [`checklist.md`](checklist.md) F411RE 항목 참조.

---

## Phase 1 미리보기 — 비트 타이밍 재계산

Phase 0 sign-off 직후 진입할 Phase 1에서 사용할 비트 타이밍 값.
**F411RE는 APB1이 50 MHz이므로 F446RE 값을 그대로 쓸 수 없다.**

### 목표
- 비트레이트 = 500 kbps (Phase 2에서 F446RE와 일치시켜야 함)
- 샘플 포인트 ≈ 87.5%

### 계산

```
CAN_Clock = APB1 = 50 MHz
50,000,000 / 500,000 = 100
→ Prescaler × (1 + BS1 + BS2) = 100
```

후보 분해:

| Prescaler | Total TQ | 샘플 포인트 후보 | 평가 |
|-----------|----------|------------------|------|
| 4 | 25 | BS1=21, BS2=3 → 88.0% | TQ 많음 (지터 여유 큼) |
| **5** | **20** | **BS1=17, BS2=2 → 90.0%** | 권장 |
| 10 | 10 | BS1=8, BS2=1 → 90.0% | **F446RE log.md와 동일한 형태** ← 채택 후보 |

> **참고:** F446RE의 Phase 1 log.md 최종 설정은 `Prescaler=9, BS1=8, BS2=1, Total TQ=10` (APB1=45MHz 기준). F411RE에서 동일한 TQ 분배를 유지하려면 **Prescaler만 9 → 10으로 변경**하면 된다. 샘플 포인트 90%가 같고, "두 노드의 비트 시간 구조가 같다"는 의미상 일관성이 있어 Phase 2 디버깅 시 비교가 쉽다.

### Phase 1에서 입력할 값 (잠정)

| 파라미터 | F446RE (참고) | **F411RE** |
|----------|--------------|-----------|
| Prescaler | 9 | **10** |
| Time Quanta in Bit Segment 1 | 8 | **8** |
| Time Quanta in Bit Segment 2 | 1 | **1** |
| ReSynchronization Jump Width | 1 | **1** |

검산: 50,000,000 / (10 × (1 + 8 + 1)) = **500,000 bps** ✓

### Phase 1에서 재사용할 함정 회피 (F446RE log.md에서 학습)

F411RE Phase 1에서 동일 패턴이 재발할 가능성이 높음:

1. **PA11 풀업 강제** — `HAL_CAN_MspInit`의 USER CODE 1 블록에서 PA11에 `GPIO_PULLUP` 적용 필수. CubeMX GUI에서 AF 핀 Pull 설정은 무시됨.
2. **Operating Mode = Loopback** 재확인 — `.ioc` 비트 타이밍 수정 시 Mode가 Normal로 회귀할 수 있음. Generate Code 후 `can.c` diff 확인.
3. **비트 타이밍 검산** — 위 50,000,000 / (Pre × Total TQ) = 500,000 등식이 성립하는지 코드 측에서 한 번 더 검증.

Phase 1 진입 전 본 항목들을 phase1 체크리스트에 명시할 것.