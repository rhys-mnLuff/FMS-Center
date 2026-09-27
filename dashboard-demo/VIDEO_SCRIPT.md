# FMS — video script (pre-recorded)

Weighted toward the model: the hardware is context, the prediction is the story.
Roughly half the runtime sits on the algorithm.

Voiceover recorded separately from picture. Word counts timed at ~150 wpm.

---

## Shot list

| # | Time | Picture | Audio |
|---|---|---|---|
| 1 | 0:00–0:12 | `field-site.jpg`, slow push | VO 1 |
| 2 | 0:12–0:29 | `context-storm-field` → `context-paddy-farmers` | VO 2 |
| 3 | 0:29–0:46 | `field-deployed` → `unit-panel` → `unit-detail` | VO 3 |
| 4 | 0:46–1:02 | `bench-calibration.mp4` | VO 4 |
| 5 | 1:02–2:02 | **Screen: pipeline panel** (capture A) | VO 5 |
| 6 | 2:02–2:28 | **Screen: prediction firing** (capture B) | VO 6 |
| 7 | 2:28–2:46 | Screen: Provenance (capture C) | VO 7 |
| 8 | 2:46–2:59 | `field-site.jpg`, wider | VO 8 |

Runtime **2:59**. Shots 5 and 6 run 86 seconds together — 48% of the video.

---

## Voiceover

**VO 1** — 0:00–0:12

> A flood gives you warning. It's in the soil, hours before it's on the surface.
> The hard part was never predicting it. It's that nobody was measuring the
> ground.

**VO 2** — 0:12–0:29

> One storm last season: six hundred and ninety million pesos of crop damage in
> the Philippines. The telemetry that could have seen it coming runs over a
> thousand dollars a site. Smallholders have never been able to buy it.

**VO 3** — 0:29–0:46

> So we built the sensor. Solar, radio-linked, about a hundred and twenty
> dollars a node, one SIM for a whole farm. But the hardware is only how we get
> the data. What matters is what runs on top of it.

**VO 4** — 0:46–1:02

> And here's the signal most systems miss. Water on the surface is the last
> stage of a flood, not the first. Rain falls, soil saturates, then water pools.
> Each step is earlier, and less obvious, than the one after it.

**VO 5** — 1:02–2:02 — *the model*

> So the system doesn't wait for water. It models the field.
>
> Three probes, and they don't agree on direction — water reads up when wet,
> soil and rain read down. First the model normalises them onto one scale, zero
> dry to one wet, anchored to values we measured on a real soil sample.
>
> Then it fits a regression, continuously, per node, across the last two dozen
> readings. Not a threshold — a rate. How fast is this specific field taking on
> water, right now.
>
> Then it fuses that with live meteorological data. The forecast doesn't tell us
> how much the soil will wet; nobody has calibrated that. It tells us how long
> the current rate survives. Rain expected, the rate holds. Forecast clears, it
> decays away.
>
> What comes out is a time. Minutes to saturation, minutes to standing water,
> and a confidence value from how well the model actually fits.

**VO 6** — 2:02–2:28 — *the model firing*

> Watch the state field. The node reads Watch — nothing has crossed a flood
> threshold — and the model is already calling saturation in two minutes.
>
> Soil crosses four three six. Water passes thirty. The alert fires with a map
> pin.
>
> The model was ahead of the event. Every alert this system sends, it sends
> early.

**VO 7** — 2:28–2:46

> And where the model doesn't know, it says so. The gap between saturation and
> standing water has never been timed, so we report it unknown rather than
> guess. Every estimated figure is labelled estimated.

**VO 8** — 2:46–2:59

> The same answer a thousand-dollar station gives — how long do I have — from a
> hundred and twenty dollars of sensor, and a model that runs on the base
> station.

---

## Screen captures

Record at **1400px wide or more**. Hard-refresh before each; to clear old alerts
from the event log, open the browser console and run
`localStorage.clear(); location.reload()`.

**A — the model** (~60s, the centrepiece)
*How the prediction works*, five boxes. Start the flood sequence first so the
numbers are live, then hold on the panel. In the edit, one continuous slow push
left to right across the boxes, landing on box 5 as VO 5 reaches "what comes out
is a time". Don't cut between boxes.

**B — the model firing** (~30s)
Press **▶ Run flood sequence**. Frame the Flood Prediction panel with the
telemetry table visible above it, so the state column and the prediction are in
the same shot. The beat you need is **step 4 of 7**: the big number reads ~2 min
while the state still says Watch. Let it run through steps 5 and 6.

If no lead time appears, the node hasn't built enough readings since the reset —
run the sequence once, stop it, start again.

**C — provenance** (~18s). Static, slow push.

---

## Burn-in captions

- `3 sensor streams → 1 normalised scale` — over box 2 in capture A
- `Regression fitted per node, live` — over box 4
- `Saturation in 2 min — state still WATCH` — the capture B money shot
- `Measured · Estimated · Not yet tested` — over capture C

---

## Disclosure

Lower third over shot 3 or 4, once:

> Sensor readings simulated. Ranges from our own bench calibration.

Don't crop the `DEMO` tag or the demo banner out of the flood-sequence footage.

---

# The AI framing

## What you can truthfully claim

The system fits a **least-squares regression model** to each node's live data
stream and fuses it with an external forecast feed. Regression is a statistical
model fitted to data — it is chapter one of every machine-learning textbook.
So these are all accurate:

- "a regression model, fitted continuously to live sensor data"
- "multi-sensor fusion with external forecast data"
- "a predictive model, not a threshold alarm"
- "real-time model fitting, per node"
- "algorithmic prediction with a confidence bound"

**The strongest single sentence you have:**

> A regression model, fitted in real time to each node's own data, fused with
> live meteorological data to predict time-to-flood.

Every word of that is true and it describes genuine modelling work.

## What you cannot claim

- ❌ "trained on" — there is no training corpus. The model is fitted at runtime
  to a rolling window and thrown away. Nothing persists, nothing accumulates.
- ❌ "it learns" / "gets smarter over time" — it does not. Same data in, same
  answer out, every time.
- ❌ "neural network" / "deep learning" / "AI-powered"
- ❌ any accuracy percentage — nothing has been validated against a real flood
- ❌ that the healthy band or the fast-drop rate was measured

## If a judge pushes: "but is it AI?"

> It's a fitted model rather than a trained one. We run least-squares regression
> on each node's live stream and fuse it with forecast data to project
> time-to-flood. We deliberately didn't go further into machine learning,
> because we have one soil type, one node and one calibration round — there
> isn't enough data to train anything honest, and a model that is confidently
> wrong about a flood is worse than no model at all. The path is there: log one
> real wetting event and the rate constant we currently leave switched off
> becomes fittable.

That answers it, shows you know where the line is, and turns the gap into
judgement — which is what is actually being scored.
