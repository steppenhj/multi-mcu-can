# CAN 프로토콜 명세

multi-mcu-can 프로젝트의 버스 구성과 메시지 사전.

---

## 버스 구성

| 파라미터 | 값 | 비고 |
|----------|----|------|
| 표준 | CAN 2.0A (11비트 identifier) | CAN-FD는 향후 마이그레이션을 위해 예약 |
| Bitrate | 500 kbps | 자동차 일반 기본값 |
| Sample point | 87.5% | 500 kbps 권장값 |
| 종단 저항 | 버스 양쪽 물리적 끝단에 120Ω | 총 버스 임피던스 ≈ 60Ω |
| 최대 버스 길이 | 500 kbps에서 < 40m | 벤치 설정은 < 1m |

---

## 메시지 사전

### `0x010` — F446RE Heartbeat

| 바이트 | 필드 | 타입 | 설명 |
|--------|------|------|------|
| 0–3  | counter      | uint32 LE | 100ms마다 증가, 2^32에서 wrap |
| 4    | flags        | uint8     | bit 0: hardware fault, bit 1: peripheral fault, bit 2–7: reserved |
| 5    | uptime_s_lsb | uint8     | 가동 시간(초), low byte |
| 6    | uptime_s_msb | uint8     | 가동 시간(초), high byte |
| 7    | reserved     | uint8     | 0x00 |

DLC: 8. 주기: 100ms. 애플리케이션 트래픽 중 최고 우선순위.

### `0x011` — F411RE Heartbeat

`0x010`과 동일한 레이아웃.

### `0x100` — F446RE Status (placeholder)

미래 actuator 관련 telemetry를 위해 예약. Phase 2–5에서는 검증용 결정론적 test pattern(예: 바이트 0–7 = sawtooth)으로 채워진다.

DLC: 8. 주기: 50ms.

### `0x200` — F411RE Sensor Data (placeholder)

미래 sensor telemetry를 위해 예약. `0x100`과 동일한 test pattern 관례 사용.

DLC: 8. 주기: 50ms.

### `0x7E0` — RPi Diagnostic Request

UDS 스타일 diagnostic master request, 모든 노드에 broadcast.

| 바이트 | 필드 | 설명 |
|--------|------|------|
| 0    | target node | 0xE8 = F446RE, 0xE9 = F411RE, 0xFF = broadcast |
| 1    | service ID  | 0x10 = session control, 0x11 = ECU reset, 0x22 = read DID |
| 2–7  | payload     | 서비스별 |

DLC: 8. 이벤트 기반 (주기 없음).

### `0x7E8` / `0x7E9` — Diagnostic Response

`0x7E8`은 F446RE, `0x7E9`는 F411RE에서 송신. 레이아웃은 ISO-15765-2 single-frame 형식을 따름:

| 바이트 | 필드 | 설명 |
|--------|------|------|
| 0    | PCI            | upper nibble = 0 (single frame), lower nibble = data length |
| 1    | service ID + 0x40 | positive response, 또는 negative response의 경우 0x7F + NRC |
| 2–7  | payload        | 서비스별 |

---

## 우선순위 근거

CAN의 arbitration은 dominant-low 방식: 낮은 숫자 ID가 버스를 점유한다. 할당은 이를 반영한다:

- **`0x010`–`0x011` (Heartbeat)** — 최고 우선순위. 버스가 혼잡해도 노드 생존 telemetry는 반드시 전달되어야 한다. Heartbeat 소실은 gateway가 노드 장애를 감지할 수 있는 유일한 신뢰할 수 있는 신호다.
- **`0x100`–`0x200` (Application data)** — 중간 우선순위. 주기적이지만 단일 사이클 지연은 허용.
- **`0x7E0`–`0x7E9` (Diagnostics)** — 최저 우선순위. 이벤트 기반, 요청-응답 방식, time-critical path에 절대 없음.

---

## 타이밍 예산 (Phase 4 목표)

500 kbps에서 표준 8바이트 프레임은 버스에서 약 220 µs 소요.

주기 부하: heartbeat (2 × 10 Hz) + status/sensor (2 × 20 Hz) = 60 프레임/초 ≈ 13 ms/초 버스 점유 = **버스 부하 1.3%**. Diagnostic burst를 위한 여유 충분.

Phase 4에서는 `0x010`과 `0x011`의 실제 jitter를 측정하여, 정상 부하에서 5ms 미만, diagnostic burst 중에도 20ms 미만을 유지하는지 검증한다.

---

## 향후 확장 (Phase 0–5 범위 밖)

- **CAN-FD 마이그레이션** — F446RE는 FDCAN을 지원하지만 F411RE는 지원 안 함. F411RE 노드 교체 필요.
- **ISO-TP multi-frame** — 현재 모든 diagnostic response는 8바이트에 맞음. Payload가 이를 초과할 때 multi-frame 지원 추가.
- **UDS 서비스 세트 확장** — 현재 $10, $11, $22만 계획. Programming session ($27, $34, $36, $37)은 부모 프로젝트와의 OTA 재통합을 위해 예약.
