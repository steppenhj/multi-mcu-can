# CAN Protocol Specification

Bus configuration and message dictionary for the multi-mcu-can project.

---

## Bus Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| Standard | CAN 2.0A (11-bit identifiers) | CAN-FD reserved for future migration |
| Bitrate | 500 kbps | Common automotive default |
| Sample Point | 87.5% | Recommended for 500 kbps |
| Termination | 120Ω at both physical bus ends | Total bus impedance ≈ 60Ω |
| Maximum bus length | < 40m at 500 kbps | Bench setup is < 1m |

---

## Message Dictionary

### `0x010` — F446RE Heartbeat

| Byte | Field | Type | Description |
|------|-------|------|-------------|
| 0–3  | counter   | uint32 LE | Increments every 100ms, wraps at 2^32 |
| 4    | flags     | uint8     | bit 0: hardware fault, bit 1: peripheral fault, bit 2–7: reserved |
| 5    | uptime_s_lsb | uint8 | Uptime seconds, low byte |
| 6    | uptime_s_msb | uint8 | Uptime seconds, high byte |
| 7    | reserved  | uint8     | 0x00 |

DLC: 8. Cycle: 100ms. Highest priority of application traffic.

### `0x011` — F411RE Heartbeat

Identical layout to `0x010`.

### `0x100` — F446RE Status (placeholder)

Reserved for future actuator-related telemetry. In Phase 2–5, populated with deterministic test pattern (e.g., bytes 0–7 = sawtooth) for verification.

DLC: 8. Cycle: 50ms.

### `0x200` — F411RE Sensor Data (placeholder)

Reserved for future sensor telemetry. Same test-pattern convention as `0x100`.

DLC: 8. Cycle: 50ms.

### `0x7E0` — RPi Diagnostic Request

UDS-style diagnostic master request, broadcast to all nodes.

| Byte | Field | Description |
|------|-------|-------------|
| 0    | target node | 0xE8 = F446RE, 0xE9 = F411RE, 0xFF = broadcast |
| 1    | service ID  | 0x10 = session control, 0x11 = ECU reset, 0x22 = read DID |
| 2–7  | payload     | Service-specific |

DLC: 8. Event-driven (no cycle).

### `0x7E8` / `0x7E9` — Diagnostic Responses

`0x7E8` from F446RE, `0x7E9` from F411RE. Layout follows ISO-15765-2 single-frame format:

| Byte | Field | Description |
|------|-------|-------------|
| 0    | PCI       | Upper nibble = 0 (single frame), lower nibble = data length |
| 1    | service ID + 0x40 | Positive response, or 0x7F + NRC for negative |
| 2–7  | payload   | Service-specific |

---

## Priority Rationale

CAN's arbitration is dominant-low: lower numerical ID wins the bus. Allocation reflects this:

- **`0x010`–`0x011` (Heartbeats)** — Highest priority. If the bus is congested, liveness telemetry must still get through. Loss of heartbeat is the gateway's only reliable signal of node failure.
- **`0x100`–`0x200` (Application data)** — Mid priority. Periodic but tolerant of single-cycle delays.
- **`0x7E0`–`0x7E9` (Diagnostics)** — Lowest priority. Event-driven, request-response, never in time-critical path.

---

## Timing Budget (Phase 4 target)

At 500 kbps, a standard 8-byte frame takes approximately 220 µs on the wire.

Periodic load: heartbeats (2 × 10 Hz) + status/sensor (2 × 20 Hz) = 60 frames/sec ≈ 13 ms/sec of bus time = **1.3% bus load**. Plenty of headroom for diagnostic bursts.

Phase 4 will measure actual jitter on `0x010` and `0x011` and verify it stays below 5 ms under normal load and below 20 ms during diagnostic bursts.

---

## Future Extensions (out of scope for Phase 0–5)

- **CAN-FD migration** — F446RE supports FDCAN; F411RE does not. Would require replacing F411RE node.
- **ISO-TP multi-frame** — Currently all diagnostic responses fit in 8 bytes. Multi-frame support added when payloads exceed this.
- **UDS service set expansion** — Currently only $10, $11, $22 planned. Programming session ($27, $34, $36, $37) reserved for OTA reintegration with parent project.