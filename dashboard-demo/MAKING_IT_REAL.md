# From demo to deployed system

What the console currently shows, what is actually behind it, and the order of
work to close the gap. Dates reference the goals already in `ROADMAP.md`.

---

## 1. Where things actually stand

| Layer | State |
|---|---|
| Sensor hardware | Built. Soldered HC-12 radios, SIM module, probes |
| Radio link | Working, 340m verified in saturated paddy |
| `Sensor_code` | Reads probes, transmits `<NODE_ID,soil,water>` |
| `Base_code` | Receives, thresholds, sends SMS, posts to ThingSpeak |
| Soil calibration | **One round.** Dry 875.0 (SD 23.0), saturated 385.4 (SD 25.1), n=5 each |
| Water calibration | **None.** Direction confirmed, no depth mapping |
| Rain gauge | **No hardware.** Entirely modelled |
| Alert levels L0–L3 | **Console only.** Firmware fires a single threshold |
| Prediction / forecast fusion | **Console only.** Firmware does not forecast |
| Advisory text | **Console only, templated** |
| Dashboard | **Simulated.** No live node has ever fed it |

The honest summary: the hardware and the radio work. The calibration is one
round of five readings on one soil type. Everything above the threshold check
— levels, prediction, forecast fusion, advisory — exists only in the browser.

---

## 2. Critical path

One item blocks more than anything else:

> **Log a real wetting event: readings and timestamps, from rain starting
> through to standing water.**

It is the only way to get the soil drop rate, the saturation→standing-water
interval, and any means of checking a prediction against what happened. Until
it exists:

- `RAPID_DROP_CALIBRATED` stays `false` and L2 cannot fire on rate
- the saturation→water stage stays labelled `untimed`
- no accuracy figure can be quoted, in a competition entry or anywhere else

Everything in Phase 2 depends on it. It needs one node in a field and one
storm, so it is bounded by weather, not by effort — which is exactly why it
should start first.

---

## 3. Phase 1 — Calibration (blocks everything)

From the Findings Brief's own open-work list.

| # | Task | Effort | Output |
|---|---|---|---|
| 1.1 | Field capacity: soak, drain 24–48h, 5 readings | 2 days elapsed, ~1h work | Replaces the **estimated** 552–699 healthy band |
| 1.2 | Soil rounds 2 and 3, dry / FC / saturated | 2 × half day | n=15 per state; tightens the ±2SD bands |
| 1.3 | **Log one real wetting event** | 1 install + 1 storm | Drop rate; saturation→water interval |
| 1.4 | Water probe at set depths (1cm, 2cm, 4cm) | Half day, bench | Turns "water contact" into a depth |
| 1.5 | Rain probe under light and heavy spray | Half day, bench | Light-vs-heavy, currently on/off only |
| 1.6 | Idle readings, all probes, 24h | 1 day passive | Confirms the 30 and 980 buffers |

**After Phase 1** the `est` markers come off the healthy band, the fast-drop
constant can be set, and the rain gauge can report intensity instead of a
boolean.

---

## 4. Phase 2 — Firmware

Bring `Base_code` up to what the console already models.

| # | Task | Effort | Notes |
|---|---|---|---|
| 2.1 | Replace placeholder thresholds with the brief's values | 1h | soil 436, water 30, rain 980, clear at 470 |
| 2.2 | Implement L0–L3 instead of one threshold | 1 day | Port `assessLevel()` — it is already written and tested in JS |
| 2.3 | Sustain counting, 3 readings | 2h | Firmware currently fires on the first crossing |
| 2.4 | **Per-node SMS cooldown** | 3h | One `lastSmsTime` today, so a second flooding node is silently suppressed |
| 2.5 | Rolling mean over 10 readings before rate | 3h | The brief's ~5% wobble makes single-reading rates meaningless |
| 2.6 | Check the HTTP response from upload | 2h | Firmware never reads it — uploads can fail silently |
| 2.7 | Log `AT+CSQ` signal strength | 2h | Command is supported, never called |
| 2.8 | Battery voltage divider + ADC read | 1 day + parts | No battery sensing exists; the console's battery is invented |
| 2.9 | Soil fault detection (≥960 = probe out) | 1h | Currently reads as "very dry" |

**2.4 is the one to do first.** It is a correctness bug, not a feature: during
a real regional flood — the exact scenario this exists for — the second and
third nodes to flood are suppressed for an hour.

---

## 5. Phase 3 — Backend

ThingSpeak accepts the data but can't drive the console.

| # | Task | Effort | Notes |
|---|---|---|---|
| 3.1 | Ingest endpoint, same query-string format | 1 day | `Base_code` already has a configurable `SERVER_HOST` — no firmware change |
| 3.2 | Time-series storage | 1 day | Postgres/Timescale or SQLite to start; a node is ~8.6k readings/day at 10s |
| 3.3 | Point the console at the API | 1 day | Swap the simulator for `fetch`; the render layer is unchanged |
| 3.4 | Run the prediction server-side | 2 days | Move `floodForecast()` out of the browser so alerts fire with nobody watching |
| 3.5 | Offline detection from upload gaps | 2h | Already dashboard-side inference; same logic server-side |
| 3.6 | Hosting | Half day | Fly/Railway/VPS, ~$5–10/mo |

**3.4 matters more than it looks.** Today the prediction only exists while a
browser tab is open. For it to be a product it has to run without one.

---

## 6. Phase 4 — Deployment

Against `ROADMAP.md`'s 2026-12-15 goal.

| # | Task | Notes |
|---|---|---|
| 4.1 | Finalise panel wattage and battery capacity | `HARDWARE.md` flags both as unspecified |
| 4.2 | Itemised BOM behind the $120 target | Currently a ceiling, not a costed bill |
| 4.3 | Enclosure survives a full wet season | Weatherproofing is pilot objective 01 |
| 4.4 | Install 1 node, 2+ weeks, logged | The roadmap's own success criterion |
| 4.5 | Local maintainer services it from the docs alone | 2027-03-31 goal; the real test of 4.2 |

---

## 7. The AI question

The current prediction is **not machine learning** — calibrated thresholds plus
a least-squares regression fitted per node at runtime. The fit is discarded
each window; nothing is trained, nothing accumulates.

Phase 1.3 is what changes that. With logged events you could honestly:

- fit the soil drop rate from data instead of leaving it unset
- learn per-site infiltration curves — different soils respond differently
- replace the fixed decay constant with one fitted to observed dry-back

That is still classical statistics rather than deep learning, and it is the
right size for the data. **Roughly 20–30 logged wetting events** across sites
would support it. One storm season at three nodes is plausibly enough.

Until then: *"a regression model fitted in real time to each node's data,
fused with live meteorological data"* is accurate. *"Trained"* is not.

---

## 8. Order of work

```
1.3 log a wetting event  ──────────────► unblocks everything
2.4 per-node cooldown    ──────────────► correctness bug, do now
1.1 field capacity       ──┐
1.2 soil rounds 2 and 3  ──┼──────────► removes the "est" markers
2.1 real thresholds      ──┘
3.1 → 3.3 backend        ──────────────► console on real data
2.2 → 2.3 levels         ──────────────► firmware matches the model
3.4 server-side predict  ──────────────► alerts without a browser
4.x deployment
```

Two things are cheap and unblock the rest: **start logging a wetting event**,
and **fix the per-node cooldown**. Neither waits on anything else.

---

## 9. Cost

| Item | Estimate |
|---|---|
| Phase 1 calibration | ~$0, bench time |
| Battery sensing parts (2.8) | <$5/node |
| Hosting (3.6) | $5–10/mo |
| SIM data | Existing |
| 3-node pilot | ~$360 at the $120 target |

Nothing here needs funding. It needs a storm, a weekend of bench work, and
about two weeks of software.
