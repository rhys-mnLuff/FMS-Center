# Calibration Data Log

Raw sensor readings from the 3-point calibration protocol described in [`CALIBRATION.md`](CALIBRATION.md). This log exists specifically so the calibration methodology is checkable, not just claimed — every number here is a logged bench reading, not an estimate.

## Run: 2026-08-31, replicate 1

| Category | n | Mean (raw) | Min | Max | SD |
|---|---|---|---|---|---|
| DRY | 5 | 875.0 | 837 | 895 | 23.0 |
| SATURATED | 5 | 385.4 | 361 | 418 | 25.1 |
| FIELD_CAPACITY | 0 | — | — | — | — |

**Status: incomplete.** FIELD_CAPACITY has zero readings logged as of this writeup — it requires a 24–48 hour gravity-drain period after saturating the soil, so it can't be rushed. Do not set `TRIGGER_PCT`/`RESET_PCT` in the firmware from DRY and SATURATED alone; the alert scale is anchored on FIELD_CAPACITY (0%) to SATURATED (100%), not DRY to SATURATED (see rationale in `CALIBRATION.md`).

Units are raw ADC counts from the sensor, not a normalized percentage — lower raw value = wetter soil on this sensor (confirmed by the base station firmware bug fix noted in `CALIBRATION.md`: the original threshold direction was backwards).

## Next step

Complete the FIELD_CAPACITY leg of replicate 1, then run replicates 2 and 3 with fresh soil samples per the protocol, before deriving real `TRIGGER_PCT`/`RESET_PCT`/`SUSTAIN_COUNT` values for the firmware. Update this file with each new run rather than overwriting — the full raw log across replicates is what makes the eventual threshold defensible in a competition Q&A or partner review.
