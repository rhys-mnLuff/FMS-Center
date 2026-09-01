# Soil Moisture Sensor: Calibration Protocol & Alert Logic

**Status:** in progress, waiting on SIM card to wire up actual SMS send end-to-end.

## Purpose

FMS uses a soil moisture sensor as an early flood indicator: heavy rain saturates soil before runoff/flooding becomes visible. The sensor's raw ADC output needs to be converted into a scientifically defensible trigger value before it can gate an SMS alert to farmers/municipalities.

## Calibration protocol (3-point, replicated)

A simple dry/wet-in-a-cup calibration is not sufficient for a real trigger value. Use actual soil and three reference points:

1. **DRY** — soil air-dried or oven-dried (200°F, ~20 min) to remove all moisture. Take 5 readings, average. This is the 0% floor, not the alert reference.
2. **FIELD CAPACITY** — soak soil fully, then let it gravity-drain 24–48 hrs (covered to prevent air-drying, uncovered at the bottom to drain). Once dripping stops, that's field capacity: the "normal, rained-on but fine" state. Take 5 readings, average. This is the 0% reference for the alert scale.
3. **SATURATED** — soak soil again but do not let it drain; keep standing water on top. This is the flood-risk state where soil can't absorb more. Take 5 readings, average. This is the 100% reference / alert threshold zone.

Repeat all three steps 3x with fresh soil samples, average across trials, and keep every raw reading (not just averages) for a defensible methodology writeup.

Soil used should be representative of the actual deployment site where possible.

## Why field capacity, not "dry," is the 0% baseline

Framing the alert scale as dry-to-saturated makes normal moist soil after routine rain look artificially "high" and risks false alerts. Framing it as field-capacity-to-saturated (see `toRelativePct()` in the firmware) means 0% = ordinary rained-on soil and 100% = actual flood-risk saturation, which is a much more meaningful scale for a flood warning trigger.

## Calibration data collection tool

File: `moisture_calibration_logger.ino` (not yet exported to this repo — see Known Gaps in the root README). Keypress-driven logger built for this exact protocol: `d`/`f`/`s` logs the current reading into DRY/FIELD_CAPACITY/SATURATED, `n` tags a new replicate, `a` prints mean/min/max/sample-SD per category, `r` dumps the full raw log as CSV to paste into a spreadsheet. The averages command directly tells you what to paste into `rawDry`/`rawFC`/`rawSat` in the alert logic sketch below.

## Alert decision logic (implemented, not yet wired to SMS)

Firmware files (not yet exported to this repo):
- `moisture_test.ino` — basic sensor bench test
- `moisture_calibration_logger.ino` — 3-point/3-replicate data collection (see above)
- `moisture_alert_logic.ino` — full decision logic, SMS-ready stub

Problems a naive single-threshold trigger has, and how the code addresses them:

- **False positives from noise/splash** → 15-sample median filter per reading.
- **Single bad reading firing an alert** → debounce: requires `SUSTAIN_COUNT` consecutive log intervals above `TRIGGER_PCT` before actually alerting.
- **Alert flapping at the boundary** → hysteresis: `RESET_PCT` is set lower than `TRIGGER_PCT`, so clearing an alert requires dropping further than triggering it.
- **Slow soaks vs. flash-flood-style rapid rain** → rate-of-rise tracking (%/min); a fast rise can flag risk even before absolute saturation is reached.
- **Probe corrosion from continuous power** → power-gating: sensor VCC wired to a digital pin (D7), only energized ~10ms per reading instead of continuously.

State machine: `NORMAL → WATCHING → ALERTED → (back to NORMAL once below RESET_PCT)`. `triggerAlert()` is a stub that currently just prints to Serial in bench tests; the SIM module AT+CMGS integration (see `firmware/sim_at_console/`) is the piece being wired into that function.

Serial output is CSV-style (`ms,raw,pct,rate_per_min,state`) so bench trials can be logged directly into a spreadsheet, and the same format is the intended shape for the "track data over time" dashboard dataset.

## Important caveat

`TRIGGER_PCT`, `RESET_PCT`, `SUSTAIN_COUNT`, and `RATE_ALERT_PCT_PER_MIN` in the current code are placeholder values, not derived numbers. Correct order of operations:

1. Run the 3-point soil calibration protocol above using `moisture_calibration_logger.ino`, get real dry/FC/saturation numbers for the target soil.
2. Log a real wetting event with `moisture_alert_logic.ino` and the CSV output.
3. Set alert parameters based on what was actually observed, not guessed values.

## Open items

- **Sensor type confirmation:** resistive (FC-28/YL-69 — needs power-gating, corrodes over time) vs. capacitive v1.2 (no corrosion, no power-gating needed). Confirm which one is in the current build and adjust wiring notes accordingly.
- **SIM integration:** wire up SIM800L/A7670, fill in `triggerAlert()` AT commands, verify actual SMS delivery end-to-end (in progress — see `firmware/sim_at_console/`).
- **Longer term:** soil moisture is a conductivity/composition-dependent proxy, not a direct flood measurement. Salinity/mineral content will shift readings — worth characterizing if deploying near brackish or mineral-heavy water sources.

## Known firmware bugs found and fixed (2026-09-01)

While preparing the dashboard integration spec, two bugs were found and fixed in `base_station.ino`:
- Missing `API_KEY_NODE_2`/`API_KEY_NODE_3` declarations — a compile error.
- Soil alert threshold direction was backwards relative to actual calibration data: was `> 200`, corrected to `< 450`, since a *lower* raw reading corresponds to *wetter* soil on this sensor.

That second bug is worth stating plainly in any writeup: it's the kind of inverted-logic error that would have silently suppressed real alerts, and it was only caught because the calibration data existed to check the code against — which is the actual argument for doing the calibration protocol in the first place, not just a methodology nicety.
