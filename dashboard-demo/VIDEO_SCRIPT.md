# FMS demo script — 3:45

Screen recording. Console `localhost:8766/dev.html`, forecast page via the
header link. Record at 1400px or wider.

Before recording, clear old alerts: browser console →
`localStorage.clear(); location.reload()`

---

## Shot 1 — Console, top of page · 0:00–0:49

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

---

## Shot 2 — The cascade strip · 0:49–1:30

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

---

## Shot 3 — Advisory Layer · 1:30–2:07

Scroll down one section.

> This is what the reading becomes for the farmer.
>
> The firmware's raw alert is underneath: soil 455, water 75. That's a number
> dump — no use to someone standing in a field.
>
> The advisory layer turns it into an instruction. Which block, what's
> happening, how long they have.
>
> The technician column keeps what the farmer message drops — trend in counts
> per minute, goodness of fit, and whether ground and forecast disagree. That
> disagreement is diagnostic: soil wetting with no rain forecast is usually
> irrigation, drainage, or a failing probe.

---

## Shot 4 — How the prediction works · 2:07–3:07

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

## Shot 5 — Field Forecast · 3:07–3:45

Click **Field Forecast →** in the header. Let it load. Press **Simulate storm**.

> The same model applied per field.
>
> Regional weather is live. But a regional forecast gives every farm in the
> province the same number — and that's the gap.
>
> Under forty millimetres of rain: north block holds, it starts healthy with
> good drainage. Riverside absorbs it, it's drying out and sandy. Canal side
> reaches standing water in two hours — already wetter than ideal, and it drains
> poorly.
>
> Same sky. Three different answers. That's what putting probes in the ground
> gets you that reading a forecast never will.

---

## Notes

**The disclosure line** is in shot 1 and takes about four seconds. Say it once
and never again — a judge who notices the `DEMO` tag later has already been
told, which is very different from catching you.

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
