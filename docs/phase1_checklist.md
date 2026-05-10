# Phase 1 — F446RE bxCAN 내부 루프백 검증 체크리스트

> Phase 0 sign-off가 완료된 상태에서 시작한다. Phase 0 커밋이 존재하지 않는다면, 여기서 멈추고 Phase 0부터 끝낸다. 운영 원칙 #3 ("한 번에 하나의 레이어만 추가한다")에 따라 두 단계를 동시에 진행하지 않는다.

---

## 이 문서가 존재하는 이유

부모 프로젝트에서 한 번 학습된 교훈: **하위 레이어가 검증되지 않은 상태에서 상위 레이어를 디버깅하면, 두 레이어의 버그가 서로를 가린다.** Phase 1은 외부 트랜시버, 외부 버스, 다른 노드를 모두 배제하고 **bxCAN 페리퍼럴 자체의 설정만 검증한다.**

Phase 1이 통과되면 다음이 보장된다:

- APB1 클럭이 CAN1 페리퍼럴에 정확히 도달한다.
- 비트 타이밍 계산이 500 kbps로 정확히 떨어진다.
- 메시지 필터가 RX FIFO를 통과시킨다.
- TX 메일박스 → RX FIFO 흐름이 동작한다.

이 네 가지를 미리 잡아 두면, Phase 2에서 추가되는 새 변수는 "외부 물리 레이어"뿐이다. ACK 에러가 나면 트랜시버 배선만 보면 된다.

---

## 필요 장비

- F446RE 보드 한 대 (Phase 0 sign-off 완료된 보드)
- 검증된 USB 케이블
- ST-Link 가상 COM 포트로 접속 가능한 시리얼 모니터 (PuTTY, Tera Term, `screen`, CubeIDE 내장 콘솔 등, 나는 Tera Term 5 사용)
- 멀티미터 (이번 단계에서는 사용하지 않을 예정이지만, 보드가 따뜻해지면 즉시 측정해야 하므로 손이 닿는 곳에 둔다)

**명시적 제외:** MCP2551 트랜시버, F411RE, RPi5, CAN_H/CAN_L 배선, 종단 저항. Phase 1은 단일 보드 단독 검증이다.

---

## Step 1 — Phase 0 베이스라인 재확인

새 프로젝트를 만들기 전에, 기존 `phase0_alive`가 여전히 정상 동작하는지 확인한다. 이는 "Phase 1을 시작했을 때의 보드 상태"가 known-good이었음을 보장한다.

- [ ] `phase0_alive` 펌웨어 플래시. LED가 1Hz로 점멸하는지 확인.
- [ ] 시리얼 모니터(115200 8N1) 연결. `[F446RE] alive, t=Xms` 메시지가 1초마다 출력되는지 확인.
- [ ] `t=` 값이 단조 증가하는지 확인 (재시작이나 행 없음).
- [ ] USB 분리 → 재연결. 깨끗한 부팅 확인.

Phase 0 베이스라인이 깨져 있다면, Phase 1로 진입하지 않는다. Phase 0를 먼저 다시 검증한다.

---

## Step 2 — phase1_loopback 프로젝트 생성

`firmware/f446re_node/phase1_loopback/`에 새 프로젝트를 생성한다. **`phase0_alive`를 복사하지 않는다** — Phase 0를 그대로 보존하기 위함. ([`ioc_f446re_phase1.md`](ioc_f446re_phase1.md) Step 1 참조)

- [ ] STM32CubeIDE → File → New → STM32 Project → Board Selector → NUCLEO-F446RE.
- [ ] Project Name: `phase1_loopback`. Location: `firmware/f446re_node/phase1_loopback/`.
- [ ] "Initialize all peripherals with their default Mode?" → **`No`** 선택.
- [ ] 프로젝트가 정상 생성되고, CubeMX `.ioc` 화면이 자동으로 열림.

---

## Step 3 — 클럭, GPIO, USART2 설정 (Phase 0와 동일)

[`ioc_f446re_phase1.md`](ioc_f446re_phase1.md) Step 2 참조. 모든 설정은 Phase 0의 `phase0_alive`와 동일해야 한다.

- [ ] RCC: HSE = **BYPASS Clock Source**.
- [ ] Clock Configuration 탭: SYSCLK = **180 MHz**, **APB1 = 45 MHz**, APB2 = 90 MHz.
- [ ] CubeMX가 APB1 위반(빨간색)을 표시하지 않음.
- [ ] SYS: Debug = **Serial Wire**, Timebase = SysTick.
- [ ] PA5 (LD2): GPIO_Output Push Pull, Low, Low speed, No pull.
- [ ] USART2: Asynchronous, **115200 8N1**, RX/TX 모두 활성화.
- [ ] 핀 이름표(라벨)가 Phase 0와 동일하게 PA5=LD2, PA2=USART2_TX, PA3=USART2_RX로 표시됨.

> **중간 검증 (선택):** 여기까지 설정하고 한 번 Generate Code → 빌드 → 플래시해서 LED 점멸 + UART "alive"가 정상 동작하는지 먼저 확인하면, CAN1 추가 작업의 베이스라인이 명확해진다. 강력 권장.

---

## Step 4 — CAN1 페리퍼럴 활성화 및 설정

[`ioc_f446re_phase1.md`](ioc_f446re_phase1.md) Step 3, 4, 5 참조. 비트 타이밍 값이 정확히 일치해야 한다.

### 4-1. Mode

- [ ] `Connectivity → CAN1 → Activated` 체크.
- [ ] Master 옵션 자동 선택됨 (선택 불가능).

### 4-2. Bit Timings (500 kbps @ APB1=45MHz)

- [ ] Prescaler (for Time Quantum) = **5**
- [ ] Time Quanta in Bit Segment 1 = **15 Times**
- [ ] Time Quanta in Bit Segment 2 = **2 Times**
- [ ] ReSynchronization Jump Width = **1 Time**

검증 계산:
```
45,000,000 / (5 × (1 + 15 + 2)) = 500,000 bps ✓
샘플 포인트 = 16 / 18 = 88.9% (87.5% 권장에 근접)
```

### 4-3. Basic Parameters

- [ ] **Operating Mode = `Loopback`** ← Phase 1의 핵심
- [ ] Time Triggered Communication Mode = Disable
- [ ] Automatic Bus-Off Management = Enable
- [ ] Automatic Wake-Up Mode = Disable
- [ ] Automatic Retransmission = Enable
- [ ] Receive FIFO Locked Mode = Disable
- [ ] Transmit FIFO Priority = Disable

> Operating Mode가 `Normal`로 남아 있으면 Phase 1은 절대 동작하지 않는다. ACK가 외부에서 와야 하는데, 외부에 아무도 없기 때문이다. 첫 송신 시 TX 메일박스가 영원히 펜딩 상태에 머문다. **반드시 `Loopback` 확인.**

### 4-4. NVIC

- [ ] CAN1 관련 모든 인터럽트 항목이 **체크 해제**됨 (TX, RX0, RX1, SCE 전부).

### 4-5. 핀 할당

Pinout view에서 확인:

- [ ] PA11 = **CAN1_RX**
- [ ] PA12 = **CAN1_TX**
- [ ] PB8/PB9에 CAN1 신호가 잡혀 있지 않음 (있다면 Reset 후 PA11/PA12로 재지정)

---

## Step 5 — Project Manager 및 Code Generator

- [ ] Toolchain / IDE = **STM32CubeIDE**
- [ ] Generate peripheral init. as a pair of .c/.h files = **체크**
- [ ] Code Generator → Keep user code when re-generating = **체크 (기본값)**

---

## Step 6 — 코드 생성 및 빌드 베이스라인

- [ ] **Generate Code** (Project → Generate Code 또는 Ctrl+Shift+G).
- [ ] `Core/Inc/can.h`, `Core/Src/can.c` 파일이 생성됨.
- [ ] `main.c`에 `MX_CAN1_Init()` 호출이 추가됨.
- [ ] `SystemClock_Config()`에서 `RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4` 유지됨.
- [ ] **빌드 (Ctrl+B) → 에러 없이 `.elf` 생성됨.**
- [ ] **플래시 → LED 1Hz 점멸 + UART "alive" 메시지 그대로 출력됨.**

> 마지막 두 항목이 **Phase 1 진입 직전의 known-good 상태**다. CAN1을 켰지만 USER CODE에서 호출하지 않았으므로 동작은 Phase 0와 같아야 한다. 다르게 동작한다면 (예: alive 메시지 깨짐, LED 안 켜짐, 보드가 부팅 안 됨) 클럭 또는 핀 충돌 가능성. **여기서 멈추고 원인을 찾는다.** USER CODE 작성으로 넘어가지 않는다.

---

## Step 7 — USER CODE 작성

[`ioc_f446re_phase1.md`](ioc_f446re_phase1.md) Step 9의 골격을 USER CODE 블록에 작성한다.

- [ ] `USER CODE BEGIN Includes`에 `#include <stdio.h>`, `#include <string.h>` 추가.
- [ ] `USER CODE BEGIN PV`에 `tx_count`, `rx_count` 전역 변수 선언.
- [ ] `USER CODE BEGIN 2`에 CAN 필터 설정 (`HAL_CAN_ConfigFilter`).
- [ ] `USER CODE BEGIN 2`에 `HAL_CAN_Start(&hcan1)` 호출.
- [ ] `USER CODE BEGIN WHILE`에 500ms 주기로 TX → RX 폴링 → UART 출력 로직.
- [ ] 빌드 → 경고 0개 또는 무시 가능한 경고만 (예: 사용되지 않은 변수).

> 빌드 경고가 의외로 자주 실수를 잡아낸다. "implicit declaration of function `memcpy`"가 뜬다면 `string.h` include가 빠진 것이다. 무시하지 말고 처리한다.

---

## Step 8 — 플래시 및 동작 검증

- [ ] 플래시 → 보드 부팅.
- [ ] LED가 1Hz로 점멸 (Phase 0와 동일한 시각적 신호).
- [ ] 시리얼 모니터(115200 8N1)에서 다음 형식의 메시지가 1초마다(0.5초마다) 출력됨:

```
[F446RE] tx=1 rx=1 t=512ms
[F446RE] tx=2 rx=2 t=1024ms
[F446RE] tx=3 rx=3 t=1536ms
...
```

- [ ] **`tx`와 `rx` 값이 항상 같은 속도로 증가** (1씩, 같은 줄에서).
- [ ] `tx`가 증가하는데 `rx`가 0에 머물거나 뒤처지지 않음.
- [ ] `t=` 값이 단조 증가 (재시작이나 행 없음).
- [ ] 30초 이상 관찰. tx/rx 차이가 발생하지 않음.

> **`tx`만 증가하고 `rx`가 0이라면**: 가장 흔한 원인 두 가지 — (1) `HAL_CAN_Start()` 호출 누락 (필터 설정만 하고 시작을 안 함), (2) 필터 마스크가 0이 아닌 값으로 설정되어 ID 0x123이 통과 못함.
>
> **`tx`도 0이라면**: Operating Mode가 `Normal`로 남아있거나, `HAL_CAN_AddTxMessage`의 반환값을 안 보고 있어 실제로는 실패하는데 카운터만 올라가고 있음. 디버거로 반환값 확인.

---

## Step 9 — 문서화 및 커밋

- [ ] 시리얼 모니터 출력 30초 분량을 `docs/phase1_log.txt`에 저장 (또는 스크린샷을 `docs/photos/phase1_console.png`에).
- [ ] 비트 타이밍 계산 결과(Prescaler=5, BS1=15, BS2=2, 측정 비트레이트 500 kbps)를 `phase1_checklist.md`의 본 문서에 직접 기입하거나 별도 로그에 남김.
- [ ] `git add firmware/f446re_node/phase1_loopback/` 후 의미 있는 커밋 메시지로 커밋. 예: `phase1(f446re): bxCAN internal loopback verified, tx==rx at 500kbps`.
- [ ] 메인 브랜치로 push.

---

## 사인오프

위의 모든 체크박스가 표시되고, 로그가 커밋되고, 30초 이상의 안정적인 tx==rx 동작이 관찰되었을 때 Phase 1이 완료된다. 

이 커밋이 존재하기 전까지 **Phase 2(F446RE ↔ F411RE 직접 2노드 CAN, MCP2551 도입)를 시작할 수 없다.**

어떤 확인이 실패하면, 우회하지 않는다. Operating Mode를 Normal로 바꾸지 않는다. tx_count를 강제로 증가시키지 않는다. 멈추고, 원인을 찾고, `.ioc` 또는 USER CODE를 수정하고, Step 6부터 다시 시작한다.

---

## 주의해야 할 실패 모드

bxCAN 학습 과정에서 자주 마주치는 함정.

| 증상 | 예상 원인 | 조치 |
|------|-----------|------|
| 빌드 시 `HAL_CAN_*` 미정의 오류 | `stm32f4xx_hal_can.h` 모듈이 활성화 안 됨 | `.ioc`에서 CAN1 Activated 재확인, Generate Code 다시 |
| `tx_count`만 증가, `rx_count` = 0 | 필터 설정 누락/잘못, 또는 `HAL_CAN_Start()` 호출 누락 | 필터 마스크 = 0 확인, Start 호출 위치 확인 (필터 설정 **이후**) |
| 양쪽 다 0, TX 메일박스가 펜딩 | Operating Mode가 `Normal` (외부 ACK 대기 중) | `.ioc` → CAN1 → Operating Mode = Loopback 재확인 |
| `HAL_CAN_AddTxMessage` 가 항상 BUSY 반환 | 3개 메일박스 모두 펜딩 상태 (위와 동일 원인의 변종) | 위와 동일. Loopback이면 즉시 비워져야 한다. |
| LED 점멸 속도가 변함 | 클럭 설정이 깨짐 — APB1이 45 MHz가 아닐 가능성 | Clock Config 탭 재확인, 빨간색 표시 확인 |
| UART 메시지 글자가 깨짐 | APB1 분주비 변경됨 → USART2 보레이트 어긋남 | APB1 = 45 MHz 재확인 |
| 보드 부팅은 되지만 첫 메시지 후 행 | `HAL_CAN_AddTxMessage`의 mailbox 인자에 NULL 전달 또는 hcan1 미초기화 | 디버거로 hcan1.State 확인 (HAL_CAN_STATE_READY여야 함) |
| 비트레이트가 정확히 500 kbps가 아니라 다른 값 | Prescaler/BS1/BS2 중 하나가 다름 | 4-2 항목 다시 확인. 45M / (Pre × (1+BS1+BS2)) = 500k 검산 |

---

## Phase 2로 넘어가기 전 체크포인트

Phase 1이 완료되어도 Phase 2는 즉시 시작하지 않는다. Phase 2 시작 전 별도로:

- [ ] MCP2551 트랜시버 모듈 데이터시트 검토 (VCC, VSS, RXD, TXD, Rs, CANH, CANL 핀맵)
- [ ] F446RE 측 트랜시버 배선도 작성 (PA12 → TXD, PA11 ← RXD, 5V → VCC, GND → VSS, Rs → GND)
- [ ] F411RE Phase 0 sign-off 완료 (Phase 0 체크리스트 F411RE 항목)
- [ ] 종단 저항(120Ω × 2) 위치 결정 — 버스의 양쪽 물리적 끝단

이 항목들은 Phase 2 체크리스트에서 다시 다뤄지지만, 미리 점검해 두면 Phase 2 진입 시 막힘 없이 진행할 수 있다.