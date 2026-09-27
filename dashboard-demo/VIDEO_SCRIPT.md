# FMS — video script (pre-recorded)

Target **2:42**. Voiceover recorded separately from picture, so nothing has to
be narrated in real time and every shot can be retaken.

Word counts below are timed at ~150 wpm. If you speak faster, pad the screen
captures rather than cutting words.

---

## Assets you already have

| File | Use |
|---|---|
| `website/intro site/images/field-site.jpg` | the pilot paddy, wide — opener |
| `context-paddy-farmers.jpg` | farmers planting — stakes |
| `context-storm-field.jpg` | storm cell — stakes |
| `field-deployed.jpg` | node alone in standing water |
| `unit-panel.jpg` | node + 20W panel |
| `unit-detail.jpg` | electronics through the clear lid |
| `bench-calibration.mp4` | 11s, probe in a soil sample |
| `dashboard-demo/dev.html` | all screen capture |

Stills need motion or they read as a slideshow — slow push-in (~5% over the
shot) on every one.

---

## Shot list

| # | Time | Picture | Audio |
|---|---|---|---|
| 1 | 0:00–0:14 | `field-site.jpg`, slow push | VO 1 |
| 2 | 0:14–0:30 | `context-storm-field.jpg` → `context-paddy-farmers.jpg` | VO 2 |
| 3 | 0:30–0:52 | `field-deployed.jpg` → `unit-panel.jpg` → `unit-detail.jpg` | VO 3 |
| 4 | 0:52–1:06 | `bench-calibration.mp4`, full 11s | VO 4 |
| 5 | 1:06–1:52 | Screen: pipeline panel, capture A | VO 5 |
| 6 | 1:52–2:16 | Screen: prediction firing, capture B | VO 6 |
| 7 | 2:16–2:32 | Screen: Provenance block, capture C | VO 7 |
| 8 | 2:32–2:42 | `field-site.jpg` again, wider | VO 8 |

---

## Voiceover

**VO 1** — 0:00–0:14

> Every wet season, farmers in the Philippines lose crops to water they didn't
> see coming. Not because the flood was unpredictable. Because nobody was
> measuring the ground it started in.

**VO 2** — 0:14–0:30

> One storm last season did six hundred and ninety million pesos of
> agricultural damage. The equipment that could have given warning costs over a
> thousand dollars a site. For a smallholder with four hectares, that maths has
> never worked.

**VO 3** — 0:30–0:52

> FMS is a solar-powered sensor node. It reads soil moisture and standing water,
> relays over radio to a base station, and texts the farmer directly. No WiFi.
> No mains power. One SIM card for an entire farm. Target cost, about a hundred
> and twenty dollars a node.

**VO 4** — 0:52–1:06

> Every threshold in the system traces back to this — a real soil sample, logged
> dry and logged saturated, so a number in the field means something.

**VO 5** — 1:06–1:52 *(the algorithm — the core of the entry)*

> Water reaching the surface is the *last* thing that happens in a flood. The
> soil saturates first — and that gap is the warning.
>
> Five steps. Three probes that don't agree on direction — water reads up when
> wet, soil and rain read down — normalised onto one scale. Each reading
> classified into a band from our own calibration. Then the rate of change,
> fitted from the last two dozen readings: how fast is this field wetting, right
> now. And that rate run forward, with the live rainfall forecast deciding how
> long it holds — while rain is expected it persists, when the forecast clears
> it decays.
>
> Out: time to saturation, time to standing water, confidence.

**VO 6** — 1:52–2:16 *(over the prediction firing)*

> Watch the state field. The node is still only on Watch — nothing has crossed a
> flood threshold — and the system is already saying saturation in two minutes.
>
> Then soil crosses four three six. Then water passes thirty, and the alert
> fires with a map pin. The prediction arrived before the flood did. That's the
> entire product.

**VO 7** — 2:16–2:32

> Where we don't know, we say so. The gap between saturation and water reaching
> the probe has never been timed — we report it unknown, not guessed. The
> healthy-soil band is published data, not our bench. It's marked estimated.

**VO 8** — 2:32–2:42

> Same question a thousand-dollar station answers — how long do I have — at a
> price a smallholder can actually reach.

---

## Screen captures

Hard-refresh (⌘⇧R) before each one so the event log starts clean. Record the
browser at 1400px wide or more; below that the pipeline panel reflows to two
columns and reads badly on video.

**Capture A — the pipeline** (need ~45s)
Scroll to **How the prediction works**. Start the flood sequence so the numbers
are moving, then sit on the panel. In the edit, push slowly left to right across
the five boxes to match VO 5. Don't cut between boxes — one continuous move.

**Capture B — the prediction firing** (need ~30s)
Press **▶ Run flood sequence**, then frame the Flood Prediction panel with the
telemetry table visible above it. The beat you need is step 4 of 7, where the
big number reads ~2 min and the node's state still says Watch. Let it run
through steps 5 and 6 so saturation and the flood alert land in the same take.

If the lead time doesn't appear, the node hasn't built enough readings since the
reset — let the sequence run once, stop it, and start it again.

**Capture C — provenance** (need ~18s)
Scroll to the **Provenance** block. Static shot, slow push. Four columns:
measured, estimated, not yet tested, simulated here.

---

## Captions to burn in

Use these over the screen captures so a muted viewer still follows it:

- `Soil 657 · Water 0 · Rain 1014` — over capture A's input box
- `Saturation in 2 min — state still WATCH` — over the capture B money shot
- `Measured · Estimated · Not yet tested` — over capture C

---

## Required disclosure

Once, on screen, early — a lower third over shot 3 or 4:

> Sensor readings simulated. Ranges from our own bench calibration.

That covers the whole video. During the flood sequence the console also
substitutes a synthetic rainfall forecast — because the algorithm correctly
refuses to predict wetting the real forecast doesn't support, and on a dry day
there'd be nothing to film. It labels itself: the banner reads **"demo weather
substituted"** and the forecast tile carries a `DEMO` tag. **Don't crop either
one out.** If a judge freeze-frames, the label should be in shot.

---

# Language rules

## Say

- "predictive model", "forward projection", "sensor fusion"
- "combines three sensor streams with live forecast data"
- "time-to-event with a confidence bound"
- "thresholds from our own calibration"

## Never say

- ❌ "AI-powered" / "machine learning" / "neural network" / "trained on"
- ❌ "it learns"
- ❌ any accuracy percentage — there is no validation set, nothing has been scored
- ❌ that the healthy band or the fast-drop rate was measured

**Why.** The algorithm is deterministic: calibrated thresholds plus a linear
projection with a decay term. No model, no training data. That is honest
engineering and it beats a black box for something people rely on — but it is
not machine learning, and if the competition's AI requirement is literal, this
entry doesn't meet it. Better to know now than to be asked on stage.

If a judge asks **"is this AI?"** —

> It's a predictive model rather than a learned one. We fuse three sensor
> streams with external forecast data to project time-to-flood. We deliberately
> didn't use machine learning: we have one soil type, one node, one calibration
> round. There isn't enough data to train anything honest, and a model that's
> confidently wrong about a flood is worse than no model. The path is there —
> log one real wetting event and the rate constant we currently leave switched
> off becomes fittable.

That answers the question and reframes the gap as judgement, which is what's
actually being scored.
