# FMS demo script — 4:13

Screen recording. Console `localhost:8766/dev.html`; forecast page reached via
the header link. Record at 1400px or wider.

Before recording, clear old alerts: browser console →
`localStorage.clear(); location.reload()`

**If your cap is 3:00** — cut Shot 2 (the cascade strip) and Beat 2 (the
regional band). That loses 62 seconds and nothing structural: the thresholds
resurface in Shot 4, and the 39.6mm figure is legible on screen without
narration. Result is 3:11.

---

# PART ONE — the console

## Shot 1 · 0:00–0:42 — Top of page

Flood Prediction is already the first section. No scrolling.

> This is the FMS technician console. The telemetry is coming from our simulator
> while the field deployment finishes — everything downstream of the input is
> the production pipeline.
>
> Three nodes stream into it. Each one sends two channels, soil and water, as
> integers from zero to 1023, into a single ingest endpoint.
>
> Select a node and you get its inference. East Canal Field is at level one,
> Watch — the classifier has it wetter than ideal, but no threshold has been
> crossed. And the model is already projecting saturation in two minutes.

**Do:** click between the three node chips so the panel visibly updates.

## Shot 2 · 0:42–1:30 — The cascade strip

Stay put. Point at the four chips under the metrics.

> These four stages are the state sequence a flood moves through. Every
> threshold is derived from our calibration dataset.
>
> Rain fires below 980. Soil leaves its healthy band below 552. Saturation is
> 436 — two standard deviations off the mean of the saturated class, so
> ordinary variance in the signal can't trip it. Standing water is anything
> above 30 on a channel whose baseline is zero.
>
> The water channel is the last state to change, not the first. By the time it
> fires, the event is already underway.
>
> The state machine needs three consecutive samples to commit a transition, and
> releases through a separate hysteresis band at 470, so it can't oscillate.

## Shot 3 · 1:30–2:10 — Advisory Layer

Scroll down one section.

> This is what the inference becomes at the output layer.
>
> The default alert payload is underneath: soil 455, water 75. Serialised state,
> no interpretation — useless to the end user.
>
> The advisory layer resolves it into an instruction. Which field, what's
> happening, how long they have.
>
> The technician view retains what that message drops — the trend coefficient,
> the goodness of fit, and whether the local signal and the external forecast
> disagree. That disagreement is the diagnostic: a wetting trend with no rain in
> the forecast is almost always a local cause, not weather.

## Shot 4 · 2:10–3:10 — How the prediction works

Scroll to the five-box panel. One slow pass left to right.

> And here's the pipeline, running live.
>
> Stage one, feature normalisation. The three channels are inverted relative to
> each other — one scales up with wetness, two scale down. All three are mapped
> onto a common zero-to-one range, with the bounds taken from the calibration
> dataset.
>
> Stage two, classification. Each sample is assigned a band from that same
> dataset.
>
> Stage three, regression. A least-squares fit over a rolling window of
> twenty-four samples, recomputed per node on every cycle. Not a threshold — a
> gradient. How fast is this series moving right now.
>
> Stage four, fusion with an external forecast API. The forecast isn't weighted
> into the magnitude. It conditions the decay: while rain is in the window the
> gradient persists, and once it clears the gradient decays exponentially.
>
> The output is a projected time to each threshold, with confidence taken from
> the fit's r-squared.

---

# PART TWO — the forecast page

**Getting there:** click `Field Forecast →` in the console header — cyan, top
right, beside the clock. Let it settle ~2s; cards appear immediately, the
rainfall bars fill in when the weather lands. Then press `Simulate storm`
(header, left of the Dev console link). Everything below assumes storm mode on.

## Beat 1 · 3:10–3:18 — Whole page

Don't point at anything. Let it land.

> The same model, applied per field.

## Beat 2 · 3:18–3:32 — Regional band

Point at `Next 12h — 39.6mm` on the right of the wide top card.

> The external feed is across the top — nearly forty millimetres over twelve
> hours. But that single value is broadcast to every consumer in the region.
> It has no per-site resolution.

## Beat 3 · 3:32–3:42 — First card, North Rice Paddy

Point at the badge `Heavy rain, ground holding`, then down to
`Soil now 661 · Healthy · Water 0`.

> North block holds. Its series starts at 661, inside the healthy band, with a
> low retention coefficient — the forty millimetres passes through.

## Beat 4 · 3:42–3:57 — Middle card, East Canal Field

Point at `Flood likely`, then the big `2.0 h`, then
`Soil now 469 · Wetter than ideal · Water 12`.

> Canal side crosses the standing-water threshold in two hours. Identical input
> — but its series starts at 469, already outside the healthy band, the water
> channel is non-zero, and its retention coefficient is high.

**Your strongest moment. Hold a beat longer than feels natural.**

## Beat 5 · 3:57–4:04 — Third card, Riverside Plot

Point at `Soil now 821 · Drying out`.

> Riverside absorbs it — series at 821, trending dry, low retention.

## Beat 6 · 4:04–4:13 — Pull back across all three

Widen to all three cards. Sweep across the three outlook badges if you can.

> One input. Three different outputs. That's what per-site state gives you that
> a regional feed structurally cannot.

---

## While recording

- The `reading Xs ago` line under each field name counts up every second. Keep
  it in frame — it's what makes the page read as live rather than a screenshot.
- East Canal's soil drifts on its own. On a long take it will have moved off
  469, so read the number off the screen rather than the script.
- The section header says `REGIONAL simulated` in storm mode. Leave it visible.
  If asked: the storm profile is substituted so the divergence is visible; the
  ground data and the model are untouched.

## Notes

**Disclosure** is four seconds in Shot 1. Say it once, never again. A judge who
notices the `DEMO` tag later has already been told — very different from
catching you.

**Vocabulary is software-side throughout** — channels, ingest endpoint,
classifier, state machine, debounce, hysteresis, feature normalisation, rolling
window, least-squares regression, gradient, exponential decay, external API,
data fusion, r-squared, payload, output layer. No RF, no ADC, no enclosure.

**If asked whether it's AI:**

> A fitted model rather than a trained one. We run least-squares regression on
> each node's live stream and fuse it with forecast data to project
> time-to-flood. We didn't go further into machine learning on purpose — one
> soil type, one node, one calibration round isn't enough to train anything
> honest, and a model confidently wrong about a flood is worse than no model.
> Log one real wetting event and the rate constant we currently leave switched
> off becomes fittable.

**Never claim:** trained on · it learns · neural network · any accuracy
percentage · that the healthy band (552–699) or the fast-drop rate was measured.
