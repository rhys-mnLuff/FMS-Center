# FMS demo script — 4:09

Screen recording. Console `localhost:8766/dev.html`; forecast page reached via
the header link. Record at 1400px or wider.

Before recording, clear old alerts: browser console →
`localStorage.clear(); location.reload()`

**If your cap is 3:00** — cut Shot 2 (the cascade strip) and Beat 2 (the
regional band). That loses 53 seconds and nothing structural: the thresholds
resurface in Shot 4, and the 39.6mm figure is legible on screen without
narration. Result is 3:16.

---

# PART ONE — the console

## Shot 1 · 0:00–0:49 — Top of page

Flood Prediction is already the first section. No scrolling.

> This is the FMS technician console. Readings come from our simulator while the
> field hardware finishes bring-up — everything downstream of them is the real
> system.
>
> Three sensor nodes in a rice paddy. Each reads soil moisture and surface water
> on a 10-bit analog-to-digital converter, so every value you see is a raw count
> from zero to 1023. They transmit on 433 megahertz radio to one base station,
> which is the only unit carrying a SIM.
>
> Select a node and you get its forecast. East Canal Field is at level one,
> Watch. Soil is wetter than ideal, but nothing has crossed a flood threshold —
> and the model is already projecting saturation in two minutes.

**Do:** click between the three node chips so the panel visibly updates.

## Shot 2 · 0:49–1:30 — The cascade strip

Stay put. Point at the four chips under the metrics.

> These four stages are the sequence a flood physically follows. Every threshold
> comes from our own bench calibration.
>
> Rain starts below 980. Soil leaves its healthy band below 552. Saturation is
> 436 — the mean of our flooded samples plus two standard deviations, so normal
> sensor scatter can't trip it. Standing water is above 30, on a probe that
> idles at zero.
>
> Water at the probe is the last thing that happens, not the first. By then the
> field is already flooding.
>
> An alert needs three consecutive readings to commit, and clears through a
> separate hysteresis band at 470, so the state can't oscillate.

## Shot 3 · 1:30–2:07 — Advisory Layer

Scroll down one section.

> This is what the reading becomes for the farmer.
>
> The firmware's raw alert is underneath: soil 455, water 75. A number dump — no
> use to someone standing in a field.
>
> The advisory layer turns it into an instruction. Which block, what's
> happening, how long they have.
>
> The technician column keeps what the farmer message drops — trend in counts
> per minute, goodness of fit, and whether ground and forecast disagree. That
> disagreement is diagnostic: soil wetting with no rain forecast is usually
> irrigation, drainage, or a failing probe.

## Shot 4 · 2:07–3:07 — How the prediction works

Scroll to the five-box panel. One slow pass left to right.

> And here's the model, running live.
>
> First, normalisation. The three probes don't agree on direction — water reads
> up when wet, soil and rain read down. All three get mapped onto one scale,
> zero dry to one wet, anchored to values measured on a real soil sample.
>
> Second, classification. Each reading lands in a band from that calibration.
>
> Third, the regression. A least-squares fit across the last twenty-four
> readings, computed continuously and per node. Not a threshold — a rate. How
> fast is this specific field taking on water right now.
>
> Fourth, fusion with live meteorological data. The forecast doesn't set how
> much the soil wets. It sets how long the current rate survives: rain expected,
> the rate holds; forecast clears, it decays exponentially.
>
> Output is a time to each threshold, with a confidence value taken from the
> fit's r-squared.

---

# PART TWO — the forecast page

**Getting there:** click `Field Forecast →` in the console header — cyan, top
right, beside the clock. Let it settle ~2s; cards appear immediately, the
rainfall bars fill in when the weather lands. Then press `Simulate storm`
(header, left of the Dev console link). Everything below assumes storm mode on.

## Beat 1 · 3:07–3:15 — Whole page

Don't point at anything. Let it land.

> The same model, applied per field.

## Beat 2 · 3:15–3:27 — Regional band

Point at `Next 12h — 39.6mm` on the right of the wide top card.

> Regional weather across the top — nearly forty millimetres forecast over
> twelve hours. But a regional forecast gives every farm in the province this
> same number.

## Beat 3 · 3:27–3:37 — First card, North Rice Paddy

Point at the badge `Heavy rain, ground holding`, then down to
`Soil now 661 · Healthy · Water 0`.

> North block holds. It starts healthy at 661 and drains well, so forty
> millimetres goes straight through it.

## Beat 4 · 3:37–3:51 — Middle card, East Canal Field

Point at `Flood likely`, then the big `2.0 h`, then
`Soil now 469 · Wetter than ideal · Water 12`.

> Canal side reaches standing water in two hours. Same rain — but it's already
> wetter than ideal at 469, there's water on the probe, and it drains poorly.

**Your strongest moment. Hold a beat longer than feels natural.**

## Beat 5 · 3:51–3:58 — Third card, Riverside Plot

Point at `Soil now 821 · Drying out`.

> Riverside absorbs it. It's drying out at 821 and it's sandy.

## Beat 6 · 3:58–4:09 — Pull back across all three

Widen to all three cards. Sweep across the three outlook badges if you can.

> Same sky. Three different answers. That's what putting probes in the ground
> gets you that reading a forecast never will.

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

**Technical terms used, all accurate:** 10-bit ADC, raw counts, 433MHz, mean
plus two standard deviations, consecutive-reading debounce, hysteresis band,
normalisation, least-squares regression, exponential decay, sensor fusion,
r-squared.

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
