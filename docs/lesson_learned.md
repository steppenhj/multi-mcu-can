# Lessons Learned

A retrospective on two hardware incidents that shaped this repository's operating principles. Written in post-mortem format — facts first, attribution second, blame nowhere.

---

## Incident 1 — RPi GPIO Damage from Battery Wire Detachment

**Project:** Neuro-Drive (parent project), early phase
**Component lost:** Raspberry Pi GPIO bank (board recovered, GPIO partially unusable)

### What happened

A 7.4V LiPo battery wire detached during operation and contacted the Raspberry Pi's 3.3V GPIO header. The voltage differential exceeded the GPIO's tolerance and damaged several pins. The board continued to boot, but specific GPIO functions were permanently lost.

### Root cause

A single-point mechanical failure (loose battery terminal) had no electrical isolation barrier between it and a sensitive 3.3V logic domain. The system was designed to work; it was not designed to fail safely.

### What was assumed

- That the battery wire would stay attached because it had stayed attached so far.
- That a wire making unintended contact with a GPIO pin was an unlikely enough scenario to ignore.
- That the cost of adding physical isolation outweighed the risk.

Each assumption was reasonable in isolation. None of them survived contact with the actual operating environment.

### Corrective action taken

The architecture was redesigned to physically separate the high-voltage power domain from the logic domain. Subsequent phases (3 onward in the parent project) ran the RPi from regulated 5V via UBEC, with battery wiring routed entirely away from logic-level pins. The GPIO was treated as a fragile resource, not a free-form interface.

### Lesson encoded into this repo

> **Voltage domains do not respect intent. They respect physical layout.**
> Until the bus is fully understood, this repo runs on bench/USB power only. No battery integration is planned within Phase 0–5.

---

## Incident 2 — Servo Stall and GND Jumper Fire During F446RE Migration

**Project:** Neuro-Drive (parent project), F411RE → F446RE migration
**Date:** 2026-05-05
**Components lost:** servo motor (assumed dead), L298N motor driver (assumed damaged)
**Components survived:** RPi5, F446RE, F411RE

### Timeline

1. F446RE migration completed; firmware ported from F411RE; basic drive test passed.
2. PWM frequency tuning attempted to address audible servo noise. New frequency settings flashed.
3. On power-up with new firmware, servo emitted a continuous "buzz" but did not move. Power was not immediately cut. The condition persisted.
4. Original F411RE firmware was reflashed onto a re-wired F411RE setup. DC motor responded normally; servo did not move.
5. The L298N-to-servo GND jumper wire ignited. Power was cut.
6. Post-incident inspection: servo unresponsive on bench, GND jumper visibly burnt, L298N suspected damaged.

### Direct cause

A GND jumper wire — repeatedly bent, flexed, and re-seated across multiple migrations — had developed internal strand breakage. Its continuity tested intact when checked, but its current-carrying capacity had degraded. When the servo entered an abnormal state (stall, drawing higher-than-normal current with no rotation), the weakened GND wire became the thermal failure point.

### Root cause

Two unverified assumptions were held simultaneously, and they masked each other:

1. **The PWM signal is correct** — based on register values matching documentation.
2. **The hardware is good** — based on having worked yesterday.

When the servo failed to move, attention focused on assumption (1): firmware was rolled back, registers were re-checked, the `.ioc` was reviewed. Throughout this, the hardware was implicitly trusted because it had recently worked. The actual fault was in (2): a wire that had silently degraded over weeks of repeated handling.

The fire was not the failure. The fire was the symptom of a failure that had already happened — minutes earlier, when the buzzing servo was not immediately disconnected.

### What should have happened

At step 3 of the timeline, the servo's behavior — sound without motion — was a **signature of a stalled actuator drawing abnormal current**. Standard practice in motor systems is to cut power within seconds of observing this state. Continued power application turns a recoverable fault into a destructive one.

### Corrective actions encoded into this repo

| Action | Where it lives in this repo |
|--------|-----------------------------|
| Verify power and GND continuity before any signal-layer work | [`docs/phase0_checklist.md`](phase0_checklist.md) — entire Phase 0 |
| Replace any wire that has been bent or flexed across migrations | Phase 0 checklist, Step 2 |
| Cut power immediately on abnormal sound or heat without expected motion | Operating Principle #2 in main `README.md` |
| Add only one new layer at a time | Operating Principle #3; Phase 2 deliberately omits MCP2515 |
| Branch before any migration | Operating Principle #4 |
| Run on bench/USB power until bus is verified | `README.md` Hardware section, "No battery in this repo" |

---

## Cross-cutting Observations

Two incidents, separated by months, share a common shape:

1. **An assumption held at the system level was not verified at the physical level.** In Incident 1, the assumption was mechanical (the wire will stay). In Incident 2, the assumption was electrical (the wire still conducts). Both were treated as true because they had been true.

2. **The failure was not where attention was focused.** Incident 1's investigation initially looked at firmware behavior on the GPIO; the cause was a battery terminal nowhere near the affected pin. Incident 2's investigation focused on PWM registers; the cause was a jumper wire's internal strand integrity.

3. **The cost of a 30-second physical check was orders of magnitude lower than the cost of skipping it.** Both incidents could have been prevented by a multimeter or a visual inspection that took less time than reading this paragraph.

The discipline encoded into this repo's Phase 0 is not about being thorough. It is about acknowledging that **systems engineers who skip physical verification eventually pay for the time they saved, with interest.**

---

## Personal Reflection

There is a temptation, after losing hardware, to either over-explain the loss or to bury it. Neither is useful. The loss happened because I trusted layers I had not personally verified. The servo's buzzing went on for too long because I was busy looking at code, and code does not buzz.

What I keep from these incidents is not a checklist — checklists exist in the other documents. What I keep is a habit: when something I am working on starts behaving in a way I cannot immediately explain, the next action is not to investigate. The next action is to remove energy from the system. Investigation comes after the smoke does not.

That habit is the only thing that separates the next incident from the last one.

---

*This document is intentionally public. The repository it lives in is built around the assumption that future readers — including future me — will benefit more from honesty about what failed than from a polished narrative of success.*