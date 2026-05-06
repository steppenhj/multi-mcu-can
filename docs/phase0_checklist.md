# Phase 0 — Power & Ground Verification Checklist

> No CAN frame, no UART byte, no SPI transfer happens in this repo until every item below is signed off. This is not optional.

---

## Why this exists

The parent project lost a servo and an L298N driver during a board migration. A GND jumper caught fire. Reviewing the incident, the root cause was not a CAN bug, a UART bug, or even bad firmware — it was the assumption that hardware previously known to work was still good after a layer underneath it had changed.

Phase 0 enforces that assumption is never made again.

---

## Hardware required

- All three boards (F446RE, F411RE, RPi 5)
- Bench power supply with current limit, **or** known-good USB cables
- Multimeter capable of continuity beep + DC voltage
- Solid-core breadboard wires (avoid frayed jumpers — failure mode of the prior incident)

**Explicitly excluded:** any battery, any motor, any servo, any L298N or motor driver. Phase 0–5 are bench-only.

---

## Step 1 — Boards in isolation

For each board, in order, **with no other boards connected**:

- [ ] Inspect board visually. No bent pins, no scorch marks, no debris.
- [ ] Plug in USB power. Power LED solid.
- [ ] Measure 3.3V rail at a known test point. Within ±5% (3.13V — 3.47V).
- [ ] Measure 5V rail. Within ±5% (4.75V — 5.25V).
- [ ] Flash `phase0_alive` firmware (STM32) or boot OS (RPi).
- [ ] Confirm onboard LED blinks at 1Hz.
- [ ] Connect UART/serial monitor at 115200 8N1. Confirm "alive" message every 1s with monotonic timestamp.
- [ ] Unplug USB. Re-plug. Confirm clean boot, no garbled UART output on startup.

Each board signs off independently. Only proceed once all three pass.

---

## Step 2 — Common ground topology

Before any signal wire is connected between boards, the GND topology must be planned and verified.

- [ ] Designate **one** common GND rail on the breadboard. Mark it physically (label or colored tape).
- [ ] Run a dedicated GND wire from each board's GND pin to the common rail. **Do not daisy-chain** GND through other components.
- [ ] With all boards powered off and unplugged, beep continuity from each board's GND pin to the common rail. Resistance < 1Ω.
- [ ] Beep continuity between any two boards' GND pins through the rail. Resistance < 1Ω.
- [ ] Inspect every GND wire physically. No frayed strands, no loose terminals, no wires under tension.

> The prior incident's GND fire happened on a jumper that had been bent and flexed across multiple migrations. Wires that look fine can have broken internal strands. **When in doubt, replace the wire.**

---

## Step 3 — Power-on sequence with all boards connected (no signals yet)

Only GND lines from Step 2 are connected. No CAN, no UART, no SPI between boards yet.

- [ ] Power on F446RE alone. Measure voltage between F446RE GND and F411RE GND with multimeter — should be 0V (within mV).
- [ ] Power on F411RE. Re-measure. Still 0V.
- [ ] Power on RPi. Re-measure all GND-to-GND pairs. All 0V.
- [ ] Confirm all three boards still print "alive" messages independently over their own UART.
- [ ] Leave the system powered for 5 minutes. Touch each board casing — no warm spots beyond expected (RPi CPU area is normal). No smell. No discoloration.

---

## Step 4 — Documentation

- [ ] Photograph the GND topology on the breadboard. Save to `docs/photos/phase0_gnd.jpg`.
- [ ] Record measured voltages in `docs/phase0_log.md` with date/time.
- [ ] Note any wire that was replaced and why.

---

## Sign-off

Phase 0 is complete when every checkbox above is marked, the log is committed, and the photo is in the repo. **Phase 1 cannot begin until this commit exists.**

If any check fails, do not work around it. Stop, find the cause, fix the cause, restart Phase 0 from Step 1 of the affected board.

---

## Failure modes to watch for

Drawn from prior experience and CAN bus literature.

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| Board boots but UART silent | TX/RX swapped, or baud mismatch | Check pinmap, verify CubeMX baud config |
| LED blink rate drifts | HSI vs HSE clock misconfigured | Check RCC config in `.ioc` |
| GND-to-GND voltage > 50mV | Ground loop or marginal connection | Re-seat GND wires, check for daisy chains |
| Board warm to touch on power-up | Possible short, draw current measurement | **Cut power immediately**, inspect |
| UART garbled only at startup | Boot pin floating, or BOOT0 wired wrong | Verify BOOT0 to GND |
| Wire warm in normal operation | Undersized conductor or partial break | **Replace wire**, do not "monitor" |

The last row is the lesson from the incident. A warm wire is a wire about to fail. Replace, do not observe.