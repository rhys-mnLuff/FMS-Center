# FMS — video script

Screen recording only. Site on `localhost:8744`, console on `localhost:8766`.
No b-roll, no photo montage. Runtime **2:14**.

| # | Time | Screen | Audio |
|---|---|---|---|
| 1 | 0:00–0:20 | Site, hero → scroll to proof strip | VO 1 |
| 2 | 0:20–0:42 | Site, scroll through the capability blocks | VO 2 |
| 3 | 0:42–1:36 | Console, pipeline panel | VO 3 |
| 4 | 1:36–2:02 | Console, prediction firing | VO 4 |
| 5 | 2:02–2:14 | Site, pricing section | VO 5 |

---

**VO 1** — 0:00–0:20

> FMS is a solar-powered sensor node for farms with no internet. It reads soil
> moisture and standing water, relays over radio to a base station, and texts
> the farmer. One SIM for the whole farm, about a hundred and twenty dollars a
> node.

**VO 2** — 0:20–0:42

> Every reading gets scored zero to a hundred against that site's own wilting
> point and saturation. Text the base station and it replies with the current
> state of any field. And when a field crosses into saturation, it texts you
> without being asked.

**VO 3** — 0:42–1:36

> That's the product. This is the part that predicts.
>
> Three probes that don't agree on direction — water reads up when wet, soil and
> rain read down. The model normalises them onto one scale, anchored to values
> measured on a real soil sample.
>
> Then it fits a regression, continuously, per node, over the last two dozen
> readings. Not a threshold — a rate. How fast is this field taking on water
> right now.
>
> Then it fuses that with live weather. The forecast doesn't say how much the
> soil will wet — nobody's calibrated that. It says how long the current rate
> survives. Rain expected, the rate holds. Forecast clears, it decays.
>
> Out comes a time: minutes to saturation, minutes to standing water, and a
> confidence value from how well the model fits.

**VO 4** — 1:36–2:02

> Watch the state column. The node reads Watch — nothing has crossed a flood
> threshold — and the model already says saturation in two minutes.
>
> Soil crosses four three six. Water passes thirty. Alert fires with a map pin.
> The model was ahead of the event.

**VO 5** — 2:02–2:14

> A commercial flood station is over a thousand dollars a site. This is a
> hundred and twenty a node, no subscription, same answer — how long do I have.

---

## Capture notes

Record at 1400px or wider. Clear old alerts first: browser console →
`localStorage.clear(); location.reload()`

- **Shot 3** — *How the prediction works*. Start the flood sequence so numbers
  are live, then one continuous slow push left to right across the five boxes.
- **Shot 4** — press **▶ Run flood sequence**. Frame the prediction panel with
  the telemetry table above it so the state column is in the same shot. The beat
  is step 4 of 7: big number ~2 min, state still Watch. If no lead time appears,
  run the sequence once, stop, start again.

## Disclosure

Once, lower third, over shot 3:

> Sensor readings simulated. Ranges from our own bench calibration.

Leave the `DEMO` tag and demo banner in frame.

## Language

Say: regression model fitted to live data · sensor fusion · predictive model ·
confidence bound

Never: trained on · it learns · neural network · AI-powered · any accuracy
percentage

The fit is computed on a rolling window and discarded — there is no training
set. "Fitted," not "trained."
