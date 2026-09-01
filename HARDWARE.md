# Hardware

## Target cost

**~$120 per unit**, positioned against $1,000+ commercial flood-telemetry hardware. This is the project's cost ceiling, not yet a fully itemized/priced BOM — see Known Gaps below.

## Confirmed components in use

| Component | Role | Notes |
|---|---|---|
| Arduino-class microcontroller (node) | Reads sensors, runs filtering, transmits over HC-12 | One per sensor node |
| Arduino-class microcontroller (base station) | Aggregates node readings, runs alert state machine, drives SIM module | `base_station.ino` |
| Soil moisture sensor | Early flood indicator (soil saturates before visible runoff) | Two candidate types in play — see below |
| Water level sensor | Direct water presence/level reading | — |
| HC-12 wireless serial module (433 MHz) | Node → base station uplink | Soldered, in bench testing/review |
| SIM800L or A7670 GSM module | SMS alert delivery | Part number confirmed on the current bench unit: **A7670**. Needs its own 3.7–4.2V supply with a large (~1000µF) capacitor — pulling power from the Arduino's 5V pin causes brownouts under the module's burst current draw, which looks exactly like a firmware bug |
| Solar panel + battery (charging circuit) | Off-grid power for field deployment | Exists in the project's framing as "solar-powered"; specific panel wattage / battery capacity not yet documented here — fill in once finalized |

## Soil moisture sensor: open decision

Two sensor types are in consideration, and confirming which one is actually on the current build matters for the wiring and maintenance notes:

| Type | Pros | Cons |
|---|---|---|
| Resistive (FC-28 / YL-69) | Cheap, common | Corrodes over time from continuous power — requires power-gating (VCC on a digital pin, energized only ~10ms per reading) |
| Capacitive v1.2 | No corrosion | Slightly more expensive, no power-gating required |

**Action item:** confirm which sensor type is in the current build and update the wiring notes accordingly (see `CALIBRATION.md` Open Items).

## SIM module bring-up notes

The A7670 (and SIM7600-class modules generally) can default to 115200 baud rather than 9600 — if `AT` returns garbage characters over serial, that's the first thing to check. Full AT command bring-up sequence and a failure-mode table are documented against the actual bench session in the firmware bring-up log; the passthrough console used for that session is in [`../firmware/sim_at_console/`](../firmware/sim_at_console/).

Known failure modes during bring-up (from actual bench testing, not a datasheet):

| Symptom | Most likely cause | Fix |
|---|---|---|
| No response to `AT` at all | TX/RX swapped, or wrong baud | Swap RX/TX pins; try 115200 |
| Garbage characters | Baud mismatch | Try 115200, then 57600 |
| Resets or LED dies during send | Power brownout (the classic) | Own supply, 1000µF cap; never the Arduino 5V pin |
| `+CPIN: SIM PIN` | Card has a PIN lock | Disable the PIN in a phone first |
| `+CSQ: 99,99` | No signal / antenna not attached | Seat the antenna; move near a window |
| `+CREG: 0,2` forever | Still searching | Wait 60s; if persistent, weak 2G coverage |
| `+CREG: 0,3` | Registration denied | SIM/carrier issue, not wiring — check the card has load and 2G support |
| `AT+CMGS` hangs at `>` | Ctrl+Z (char 26) never sent | Terminal must send the actual control character, not the text "^Z" |
| Works on bench, fails outdoors | Supply sag under cold/load | Test on the actual deployment battery, not bench power |

## Known gaps

- No fully itemized, per-part-priced BOM yet — the $120 figure is a target ceiling, not a summed line-item total. This should be the next hardware doc added.
- Solar panel wattage and battery capacity not yet specified in writing.
- No enclosure specification yet for outdoor/flood-adjacent deployment.
- No wiring diagrams / schematics in this repo yet.
