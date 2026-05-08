# multi-mcu-can: CAN 2.0 Multi-MCU Distributed Communication

🇰🇷 [한국어](README.kr.md)

![C](https://img.shields.io/badge/C-STM32_HAL-A8B9CC?logo=c&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.11-3776AB?logo=python&logoColor=white)
![STM32](https://img.shields.io/badge/MCU-F446RE_%2B_F411RE-03234B?logo=stmicroelectronics&logoColor=white)
![Raspberry Pi](https://img.shields.io/badge/MPU-Raspberry_Pi_5-C51A4A?logo=raspberrypi&logoColor=white)
![CAN](https://img.shields.io/badge/Bus-CAN_2.0-green)

A focused study of **CAN 2.0 multi-node communication** across two STM32 boards and a Raspberry Pi 5. No actuators, no chassis, no application logic — just the bus, its protocol, and the discipline required to make distributed MCUs talk reliably.

This repository is a follow-up to **[Neuro-Drive](https://github.com/steppenhj/neuro-drive)**, where Phase 6 originally lived. It was extracted to strip away the actuator layer and concentrate on fundamentals.

---

## Why a Separate Repo

The parent project ran into a hardware incident during the F446RE migration: a servo stalled, a GND jumper caught fire, and the L298N driver was lost alongside the servo. The root cause was not the code — it was the assumption that previously-working hardware would still be working after a layer was changed underneath it.

This repo is built around the discipline that was missing the first time: **verify each layer before adding the next.**

---

## Operating Principles

These four rules are non-negotiable for every phase in this repo.

1. **Verify power and ground before suspecting code.** A multimeter check costs 30 seconds; a burnt board costs days.
2. **If a component makes abnormal sound or heat without expected motion, cut power immediately.** Investigate before re-applying. Stalled motors and stalled servos are silent killers — until they aren't.
3. **Add one layer at a time.** Never debug two new layers simultaneously. If the bus is new, the firmware should be old. If the firmware is new, the bus should be known-good.
4. **Keep a known-good backup. Branch before any migration.** "I can revert if it breaks" only works if the thing you're reverting to is actually clean.

---

## Architecture

Three nodes on a single CAN 2.0 bus at 500 kbps, with 120Ω termination at both physical ends.

### Phase 0 — Deployment Topology (Power & GND only)

![Phase 0 deployment diagram](docs/diagrams/deployment_phase0.png)

```
                       CAN_H ────────────────────────────────────
                                  │             │              │
                       CAN_L ─────┼─────────────┼──────────────┼──
                                  │             │              │
                            ┌─────┴────┐  ┌─────┴────┐  ┌──────┴──────┐
                            │ MCP2551  │  │ TJA1050  │  │   MCP2551   │
                            │  (xcvr)  │  │  (xcvr)  │  │   (xcvr)    │
                            └─────┬────┘  └─────┬────┘  └──────┬──────┘
                                  │             │              │
                            ┌─────┴────┐  ┌─────┴────┐  ┌──────┴──────┐
                            │ F446RE   │  │ MCP2515  │  │  F411RE     │
                            │  bxCAN   │  │  (SPI)   │  │   bxCAN     │
                            └──────────┘  └─────┬────┘  └─────────────┘
                                                │
                                          ┌─────┴────┐
                                          │  RPi 5   │
                                          │SocketCAN │
                                          └──────────┘
```

**Node roles** (deliberately abstract — no actuators yet):
- **F446RE** — placeholder for future MotorECU. For now, periodic status broadcaster.
- **F411RE** — placeholder for future SensorECU. For now, periodic data broadcaster.
- **RPi 5** — gateway, logger, diagnostic master. Uses MCP2515 + TJA1050 over SPI.

---

## Phase Plan

Each phase produces a **standalone, independently buildable, regression-testable** artifact. A later phase failing must not break the ability to run an earlier one.

| Phase | Goal | Verification | Nodes |
|:-----:|------|--------------|:-----:|
| **0** | Power & GND validation, basic life signs | Multimeter readings logged, "I'm alive" UART output from each board | 3 independent |
| **1** | F446RE bxCAN internal loopback | TX/RX counters increment in sync via UART monitor | 1 |
| **2** | F446RE ↔ F411RE direct two-node CAN (no MCP2515) | LED mirror: button on one node → LED on the other within 50ms | 2 |
| **3** | + RPi5 (MCP2515) joins the bus | `candump can0` shows both STM32 messages with correct IDs and DLC | 3 |
| **4** | Periodic + event-driven message scheduling, priority handling | Bus log analysis: jitter < 5ms on periodic msgs, no priority inversion | 3 |
| **5** | Error handling, bus-off recovery, diagnostic exchange (UDS-style) | Intentional unplug/replug scenario; node recovers within 1s | 3 |

### Why Phase 0 exists separately

The parent project's incident started with a working car and ended with a burnt servo. The fault propagated through layers nobody was checking. This repo refuses to begin Phase 1 until Phase 0 is signed off — every board powers cleanly, every GND is continuous, and every node prints "alive" over UART before any bus is introduced.

### Why Phase 2 has no MCP2515

The MCP2515 introduces an SPI-to-CAN bridge layer. Bundling that with first-time CAN bring-up means an SPI bug and a CAN bug can hide each other. Phase 2 uses only the STM32s' built-in bxCAN peripherals through transceivers — direct, minimal, debuggable. Phase 3 then adds the MCP2515 as a single new variable.

---

## CAN ID Allocation

Borrowing conventions from automotive standards (UDS / ISO-15765) for diagnostic IDs.

| ID      | Sender   | Purpose                              | Cycle  | Priority |
|---------|----------|--------------------------------------|--------|:--------:|
| `0x010` | F446RE   | Heartbeat (alive counter, fault flags) | 100ms  | High     |
| `0x011` | F411RE   | Heartbeat                            | 100ms  | High     |
| `0x100` | F446RE   | Status (placeholder for actuator data) | 50ms   | Mid      |
| `0x200` | F411RE   | Sensor data (placeholder)            | 50ms   | Mid      |
| `0x7E0` | RPi      | Diagnostic request (broadcast)       | event  | Low      |
| `0x7E8` | F446RE   | Diagnostic response                  | event  | Low      |
| `0x7E9` | F411RE   | Diagnostic response                  | event  | Low      |

**Lower CAN ID = higher priority** (CAN's natural arbitration). Heartbeats win against everything else, ensuring liveness telemetry is never delayed by application traffic.

Full message dictionary: [`docs/can_protocol.md`](docs/can_protocol.md).

---

## Repository Layout

```
multi-mcu-can/
├── README.md
├── docs/
│   ├── hardware.md            # BOM, wiring, pinmaps
│   ├── can_protocol.md        # Full message dictionary, DLC, byte order
│   ├── phase0_checklist.md    # Power/GND verification procedure
│   └── lessons_learned.md     # Incident retrospective from the parent project
├── firmware/
│   ├── f446re_node/
│   │   ├── phase0_alive/      # Blink + UART "alive"
│   │   ├── phase1_loopback/   # bxCAN internal loopback
│   │   ├── phase2_two_node/
│   │   ├── phase3_three_node/
│   │   ├── phase4_scheduling/
│   │   └── phase5_recovery/
│   └── f411re_node/
│       ├── phase0_alive/
│       ├── phase2_two_node/
│       ├── phase3_three_node/
│       ├── phase4_scheduling/
│       └── phase5_recovery/
├── rpi/
│   ├── phase3_gateway/        # SocketCAN setup, basic candump pipeline
│   ├── phase4_logger/         # Timestamped CAN log + jitter analysis
│   ├── phase5_diagnostic/     # UDS-style request/response client
│   └── tools/
│       ├── bus_load_monitor.py
│       └── jitter_analyzer.py
└── tools/
    ├── wiring_diagrams/
    └── can_id_table.csv
```

Each phase folder is independently buildable. If Phase 4 breaks, Phase 3 still flashes and runs.

---

## Hardware

| Component | Part | Role |
|-----------|------|------|
| MCU 1 | STM32 NUCLEO-F446RE | bxCAN node 1, future MotorECU |
| MCU 2 | STM32 NUCLEO-F411RE | bxCAN node 2, future SensorECU |
| MPU | Raspberry Pi 5 (4GB) | Gateway, logger, diagnostic master |
| CAN Interface (RPi) | MCP2515 + TJA1050 (SPI→CAN) | Bridges RPi SPI to CAN bus |
| CAN Transceiver (F446RE) | MCP2551 | Differential physical layer |
| CAN Transceiver (F411RE) | MCP2551 or TJA1050 | Differential physical layer |
| Termination | 2 × 120Ω resistors | At both physical ends of the bus |
| Power | Bench supply or USB only (no battery) | Phase 0–5 all run from desk power |

**No battery in this repo.** The parent project's incident involved battery wiring; until the bus is fully stable, this repo runs exclusively from regulated bench/USB power. Battery integration is explicitly out of scope.

Full BOM and wiring: [`docs/hardware.md`](docs/hardware.md).

---

## Getting Started

### Prerequisites
- STM32CubeIDE
- Raspberry Pi OS Bookworm with `can-utils` installed
- Python 3.11

### Phase 0 — Bring-up

```bash
# Each board flashed with its phase0_alive firmware:
#   - LED blinks at 1Hz
#   - UART prints "[F446RE] alive, t=12345ms" every second

# RPi side (no CAN yet):
sudo apt install can-utils python3-pip
# Verify each board's UART output independently before proceeding
```

See [`docs/phase0_checklist.md`](docs/phase0_checklist.md) for the multimeter/continuity procedure.

### Phase 3 — Three-node bus (RPi joins)

```bash
# RPi side, after MCP2515 wired to SPI0 and dtoverlay configured:
sudo ip link set can0 up type can bitrate 500000
candump can0     # should show 0x010, 0x011, 0x100, 0x200 streaming
```

---

## Roadmap (post-Phase 5)

Once the three-node bus is rock-solid, possible extensions:

- **CAN-FD** migration (F446RE supports it; F411RE does not — would require a node swap)
- **ISO-TP** (ISO-15765-2) segmented messaging for diagnostic payloads > 8 bytes
- **UDS** service implementation (read DTC, ECU reset, programming session)
- Reintegration with the parent **Neuro-Drive** chassis once actuator layer is rebuilt

---

## Author

**박해진 (Haejin Park)**  
Kyungpook National University