# FMS — demo video script

Console: `dashboard-demo/dev.html`. Everything below is driven by the
**▶ Run flood sequence** button, which runs the same seven beats every take at
4× speed (~90 seconds end to end). Press it once and narrate over it.

Before recording: hard-refresh the page so the event log starts clean.

---

## Cold open — 0:00–0:15

**On screen:** top of the console, before pressing anything.

> This is the technician console for FMS. Three solar sensor nodes in a rice
> paddy in Chiang Mai, reporting soil moisture and water level over radio to a
> single base station. No WiFi, no mains power, one SIM for the whole farm.
>
> Everything you're about to see runs on readings from those probes.

**Point at:** the fleet strip — 3/3 nodes, average soil, average water.

---

## The problem — 0:15–0:35

**On screen:** the Nodes — Raw Telemetry table.

> The sensors give you numbers like these. Soil 657. Water 0. On their own they
> mean nothing to a farmer — and by the time water actually reaches the probe,
> the field is already flooding. You've been told about a flood you can see out
> the window.
>
> The useful question isn't "is it flooding." It's "how long do I have."

---

## The algorithm — 0:35–1:20

**On screen:** scroll to **How the prediction works**. Walk the five boxes
left to right as you talk. The numbers in them are live.

> **Inputs.** Three probes, all 10-bit, all disagreeing about direction. Water
> reads *up* when wet. Soil and rain read *down*. Plus the live rainfall
> forecast for these coordinates.
>
> **Normalise.** First step is putting them on one scale — zero is dry, one is
> wet — using the dry and saturated values we measured on the bench.
>
> **Classify.** Each reading falls into a band. Soil 436 or below is saturated.
> Water above 30 is standing water. Rain below 980 means it's falling. Those
> thresholds come from our own calibration, padded past the noise so a single
> stray drop can't fire an alert.
>
> **Project.** This is the part that buys time. We fit the rate of change from
> the last two dozen readings — how fast is this field wetting, right now — and
> run it forward. The forecast decides how long that rate survives: while rain
> is falling or expected, it holds; when the forecast clears, it decays.
>
> **Output.** Time to saturation, time to standing water, and a confidence
> figure from how well the trend actually fits.

---

## The prediction firing — 1:20–2:10

**Press ▶ Run flood sequence.** Follow the banner. Beats to hit:

| Banner step | Say this |
|---|---|
| 2 · Rain begins | "The rain gauge drops below 980. Nothing has happened to the soil yet." |
| 3 · Soil wetting | "Now the ground starts taking it up. The node moves to Watch." |
| 4 · Prediction | **The money shot.** "Saturation in two minutes — and look at the state: still Watch. Nothing has crossed a flood threshold yet. This is the warning arriving before the event." |
| 5 · Saturation | "Soil crosses 436. The prediction was right." |
| 6 · Standing water | "Water passes 30. Flood confirmed, SMS fires with a map pin." |
| 7 · Draining | "It clears through a hysteresis band so the alert can't flicker." |

**Point at:** the cascade strip lighting up left to right — rain, soil, soil
saturated, water. That's the physical sequence of a flood, and the system walks
it one stage at a time.

---

## Honesty beat — 2:10–2:30

Do not skip this. It is the strongest thing in the video, and a judge will find
it anyway.

> Two things this doesn't know yet. The gap between the soil saturating and
> water reaching the probe has never been timed — so we report it as unknown
> rather than guessing. And the healthy-soil band is from clay-loam literature,
> not our own tests, so it's marked "est" everywhere it appears.
>
> Everything measured is measured. Everything estimated says so.

**Point at:** the Provenance block — measured / estimated / not yet tested /
simulated.

---

## Close — 2:30–2:45

> A commercial flood telemetry station costs upwards of a thousand dollars per
> site. Our target is about $120 a node, one SIM per farm, no per-sensor
> subscription. Same question answered — how long do I have — at a price a
> smallholder can actually reach.

---

# Language rules

## Say this

- "predictive model", "forward projection", "sensor fusion"
- "combines three sensor streams with live forecast data"
- "calculates time-to-event with a confidence bound"
- "algorithmic prediction from calibrated thresholds"

## Do NOT say this

- ❌ "AI-powered" / "machine learning" / "neural network" / "trained on"
- ❌ "it learns from the data"
- ❌ "95% accurate" — there is no validation set; nothing has been scored
- ❌ any claim that the healthy band or the fast-drop rate was measured

**Why this matters.** The algorithm is deterministic — rules from your bench
calibration plus a linear projection. There is no model and no training data.
That is defensible engineering and it beats a black box for a system people
rely on, but it is not machine learning. If the competition's AI requirement is
literal, this entry does not meet it, and it is much better to know that now
than to be asked on stage what it was trained on.

If a judge asks directly — **"is this AI?"** — the honest answer that still
lands well:

> It's a predictive model rather than a learned one. We fuse three sensor
> streams with external forecast data to project time-to-flood. We deliberately
> didn't use machine learning, because we have one soil type, one node and one
> calibration round — there isn't enough data to train anything honest, and a
> model that's confidently wrong about a flood is worse than no model. The
> path to learning is there: log one real wetting event and the rate constant
> we currently leave switched off becomes fittable.

That answer turns the weakness into judgement, which is what they're actually
scoring.

---

# What's simulated

Say this once, early, and you're covered for the whole video:

> The readings are simulated — the hardware exists but isn't logging yet. The
> ranges come from our bench calibration, so a threshold means the same thing
> here as it does in the field.

During **Run flood sequence** the console also substitutes a synthetic rainfall
forecast, because the algorithm correctly refuses to predict wetting that the
real forecast doesn't support — on a dry Manila day there would be nothing to
show. The banner says **"demo weather substituted"** while it runs and the
forecast tile is tagged `DEMO`. Leave both visible; if anyone freeze-frames,
the label is right there.
