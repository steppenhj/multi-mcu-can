# STM32CubeMX .ioc Configuration — F446RE Phase 0 (phase0_alive)

Phase 0 목표: LED 1Hz 점멸 + UART로 "alive" 메시지 1초마다 출력.  
CAN, FreeRTOS, 타이머 인터럽트 없음. 최소 구성.

---

## 1. 새 프로젝트 생성

STM32CubeIDE → File → New → STM32 Project

| 항목 | 값 |
|------|----|
| Board Selector 탭 | `NUCLEO-F446RE` 검색 후 선택 |
| Project Name | `phase0_alive` |
| Project Location | `firmware/f446re_node/phase0_alive/` |
| Targeted Language | C |
| Targeted Binary Type | Executable |
| Targeted Project Type | STM32Cube |

> Board Selector를 사용하면 NUCLEO 기본 핀(LD2, B1, USART2)이 자동 설정된다. 이후 불필요한 항목을 제거하는 게 직접 핀을 잡는 것보다 빠르다.

---

## 2. RCC — 클럭 소스

`System Core → RCC`

| 항목 | 설정 | 이유 |
|------|------|------|
| High Speed Clock (HSE) | **BYPASS Clock Source** | NUCLEO-F446RE는 크리스탈이 아니라 ST-Link MCO(8MHz)로 HSE를 공급한다. Crystal/Resonator로 설정하면 클럭이 잡히지 않는다. |
| Low Speed Clock (LSE) | Disable | Phase 0에서 RTC 불필요 |

---

## 3. Clock Configuration 탭 — PLL 설정

목표: SYSCLK = **180 MHz** (F446RE 최대)

```
HSE = 8 MHz (ST-Link MCO)
  │
  └─→ PLL Source Mux: HSE
        PLLM = 4   → VCO 입력 = 8 / 4 = 2 MHz  (권장 범위 1–2 MHz)
        PLLN = 180 → VCO 출력 = 2 × 180 = 360 MHz
        PLLP = 2   → SYSCLK  = 360 / 2 = 180 MHz
        PLLQ = 8   → (USB용, Phase 0에서 사용 안 하지만 기입해 둠)
```

| 버스 | 분주비 | 결과 | 상한 |
|------|--------|------|------|
| AHB  | /1  | 180 MHz | 180 MHz |
| APB1 | /4  | 45 MHz  | 45 MHz ← 반드시 지킬 것 |
| APB2 | /2  | 90 MHz  | 90 MHz |

> CubeMX가 APB1 위반을 빨간색으로 표시한다. APB1 분주비가 /4가 아니면 USART2 보레이트 계산이 틀린다.

**HAL Timebase Source:** SysTick (기본값 유지)  
Phase 3 이후 FreeRTOS를 붙일 때 TIM으로 옮겨야 하지만, Phase 0에서는 SysTick으로 충분하다.

---

## 4. SYS — 디버그 설정

`System Core → SYS`

| 항목 | 설정 |
|------|------|
| Debug | Serial Wire (SWD) |
| Timebase Source | SysTick |

> JTAG으로 설정하면 PA15/PB3/PB4가 잠겨 나중에 다른 용도로 못 쓴다. SWD를 고정으로 쓸 것.

---

## 5. GPIO — LED

`System Core → GPIO`

NUCLEO Board Selector 사용 시 PA5가 자동으로 잡혀 있다. 설정을 확인만 하면 된다.

| 핀 | Label | Mode | Output Level | Speed | Pull |
|----|-------|------|-------------|-------|------|
| PA5 | LD2 | GPIO_Output Push Pull | Low | Low | No pull |

> Output Level을 Low로 두면 부팅 직후 LED 꺼진 상태에서 시작한다. 코드에서 명시적으로 켜고 끄기 때문에 초기값은 무관하나 Low가 더 명확하다.

B1 버튼(PC13)은 Phase 0에서 쓰지 않지만 Board Selector가 자동으로 잡아 줬다면 그대로 둬도 무방하다.

---

## 6. USART2 — "alive" 메시지 출력

`Connectivity → USART2`

NUCLEO의 ST-Link 가상 COM 포트는 USART2(PA2=TX, PA3=RX)에 연결되어 있다.

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

DMA, 인터럽트 모두 비활성화. Phase 0는 `HAL_UART_Transmit()` 폴링으로 충분하다.

핀 확인:

| 핀 | Signal | 방향 |
|----|--------|------|
| PA2 | USART2_TX | MCU → ST-Link → PC |
| PA3 | USART2_RX | PC → ST-Link → MCU |

---

## 7. 활성화하지 말아야 할 것

| 항목 | 이유 |
|------|------|
| CAN1 / CAN2 | Phase 1부터. 지금 켜면 Phase 1에서 추가한 것인지 Phase 0에서 실수로 켠 것인지 구분이 안 된다. |
| FreeRTOS | Phase 0에 스케줄러 불필요. SysTick Timebase와 FreeRTOS가 충돌하면 HAL_Delay가 멈춘다. |
| DMA | 폴링 UART로 충분. 복잡성 추가 없음. |
| 추가 타이머(TIM2 등) | 1Hz 점멸은 `HAL_GetTick()` 기반 비교로 구현한다. 타이머 인터럽트 불필요. |
| SPI / I2C | F446RE 노드는 MCP2515를 쓰지 않는다. |

---

## 8. Project Manager 탭

| 항목 | 값 |
|------|-----|
| Project Name | phase0_alive |
| Project Location | (workspace 내 경로, 위에서 설정한 값) |
| Toolchain / IDE | **STM32CubeIDE** |
| Minimum Heap Size | 0x200 |
| Minimum Stack Size | 0x400 |
| Generate peripheral init. as a pair of .c/.h files | **체크** |

마지막 항목을 체크하면 `usart.c/h`, `gpio.c/h` 등이 분리 생성된다. `main.c` 재생성 시 충격이 줄어든다.

`Code Generator` 탭:

| 항목 | 값 |
|------|-----|
| Copy only necessary library files | 선택 (전체 HAL 복사 방지) |
| Generate peripheral init. as pair | 위와 동일 — 여기서 설정하는 게 더 정확한 위치 |
| Keep user code when re-generating | 기본 체크 (USER CODE 블록 보존) |

---

## 9. 코드 생성 후 확인 사항

Generate Code 이후 STM32CubeIDE에서:

1. `Core/Src/main.c` → `MX_USART2_UART_Init()`, `MX_GPIO_Init()` 호출 확인.
2. `SystemClock_Config()` → `HAL_RCC_ClockConfig()`에서 APB1 분주비 `RCC_HCLK_DIV4` 확인.
3. 빌드(Ctrl+B) → 에러 없이 `.elf` 생성 확인.
4. 플래시 → LED 점멸 없음이 정상 (아직 USER CODE 없음).

---

## 10. USER CODE 작성 가이드 (설정 후 작성할 것)

`.ioc` 설정 완료 후 `main.c` 의 USER CODE 블록에 작성할 내용 요약.  
(실제 코드는 `app_alive.c/h`에 분리 권장 — workflow.md 참조)

```c
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
        int len = snprintf(msg, sizeof(msg), "[F446RE] alive, t=%lums\r\n", now);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
    }
}
/* USER CODE END WHILE */
```

> `snprintf`는 `<stdio.h>` include가 필요하다. USER CODE BEGIN Includes 블록에 추가할 것.

---

## 요약

```
활성화: HSE(BYPASS) → PLL 180MHz → SysTick → PA5(LED) → USART2(115200)
비활성화: CAN, FreeRTOS, DMA, SPI, 추가 타이머
검증: 빌드 성공 + LED 1Hz + 터미널에서 "[F446RE] alive, t=Xms" 1초마다 출력
```

Phase 0 sign-off 기준은 [`checklist.md`](checklist.md) 참조.
