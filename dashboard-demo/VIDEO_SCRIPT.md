# FMS demo script — 4:22

Screen recording. Console `localhost:8766/dev.html`; forecast page reached via
the header link. Record at 1400px or wider.

Before recording, clear old alerts: browser console →
`localStorage.clear(); location.reload()`

**If your cap is 3:00** — cut Shot 2 (the cascade strip) and Beat 2 (the
regional band). That loses 62 seconds and nothing structural: the thresholds
resurface in Shot 4, and the 39.6mm figure is legible on screen without
narration. Result is 3:20.

---

# PART ONE — the console

## Shot 1 · 0:00–0:42 — Top of page

Flood Prediction is already the first section. No scrolling.

> This is the FMS console. The readings are coming from our simulator while the
> real sensors finish testing — but everything that happens to those readings is
> the real software.
>
> Three sensors send in data. Each one reports two numbers, soil and water, on a
> scale of zero to 1023.
>
> Click a sensor and you get its prediction. East Canal Field is on Watch right
> now. The soil is wetter than it should be, but nothing has crossed a danger
> line yet — and the software is already saying it will flood in two minutes.

**Do:** click between the three node chips so the panel visibly updates.

## Shot 2 · 0:42–1:30 — The cascade strip

Stay put. Point at the four chips under the metrics.

> These four steps are the order a flood actually happens in, and every number
> here came from our own testing.
>
> Rain starts below 980. The soil leaves its healthy range below 552. It counts
> as fully soaked at 436. And standing water is anything above 30, on a sensor
> that normally sits at zero.
>
> The important part is the order. Water on the sensor is the *last* thing to
> happen, not the first. By the time it shows up, the field is already
> flooding — so we watch the earlier steps instead.
>
> And an alert only fires after three readings agree, then switches off at a
> different number than it switched on, so it can't flicker back and forth.

## Shot 3 · 1:30–2:12 — Advisory Layer

Scroll down one section.

> This is what the farmer actually gets.
>
> Underneath is the plain alert the system would send on its own: soil 455,
> water 75. Just numbers. No use to anyone standing in a field.
>
> The advisory turns that into an instruction — which field, what's happening,
> and how long they have.
>
> On the right, the technician sees what that message leaves out: how fast the
> readings are moving, how reliable that is, and whether the sensor and the
> forecast agree. When they don't, that's the useful part — soil getting wetter
> with no rain usually means a leak or a broken sensor.

## Shot 4 · 2:12–3:18 — How the prediction works

Scroll to the five-box panel. One slow pass left to right.

> And here's the software doing it, step by step, live.
>
> Step one. The three sensors don't agree on which direction is wet — one counts
> up, two count down. So first we put them all on the same scale: zero is dry,
> one is soaked.
>
> Step two. Each reading gets sorted into a band, using the ranges from our
> testing.
>
> Step three, the key one. Instead of checking a number against a limit, we draw
> a line through the last twenty-four readings and measure how steep it is. That
> gives us the speed — how fast this field is taking on water, not just where it
> is.
>
> Step four. We pull in a live weather forecast. It doesn't tell us how wet the
> soil gets — it tells us how long that speed lasts. Rain still coming, the
> field keeps filling. Forecast clears, the rate fades out.
>
> Out the other end: minutes until it soaks, minutes until it floods, and how
> much to trust that.

---

# PART TWO — the forecast page

**Getting there:** click `Field Forecast →` in the console header — cyan, top
right, beside the clock. Let it settle ~2s; cards appear immediately, the
rainfall bars fill in when the weather lands. Then press `Simulate storm`
(header, left of the Dev console link). Everything below assumes storm mode on.

## Beat 1 · 3:18–3:26 — Whole page

Don't point at anything. Let it land.

> The same model, applied per field.

## Beat 2 · 3:26–3:40 — Regional band

Point at `Next 12h — 39.6mm` on the right of the wide top card.

> The weather forecast is across the top — nearly forty millimetres of rain over
> twelve hours. But that's one number for the whole region. Every farm gets told
> exactly the same thing.

## Beat 3 · 3:40–3:50 — First card, North Rice Paddy

Point at the badge `Heavy rain, ground holding`, then down to
`Soil now 661 · Healthy · Water 0`.

> North block is fine. It starts at 661, right in the healthy range, and it
> drains quickly — so all that rain passes straight through.

## Beat 4 · 3:50–4:05 — Middle card, East Canal Field

Point at `Flood likely`, then the big `2.0 h`, then
`Soil now 469 · Wetter than ideal · Water 12`.

> Canal side floods in two hours. Exact same rain — but it starts at 469, it's
> already too wet, there's water sitting on the sensor, and this block holds
> onto water instead of draining it.

**Your strongest moment. Hold a beat longer than feels natural.**

## Beat 5 · 4:05–4:13 — Third card, Riverside Plot

Point at `Soil now 821 · Drying out`.

> Riverside soaks it up. It's at 821 and drying out, so it has room to take the
> rain.

## Beat 6 · 4:13–4:22 — Pull back across all three

Widen to all three cards. Sweep across the three outlook badges if you can.

> Same rain. Three different answers. That's what measuring each field gives you
> that a regional forecast never can.

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

**Plain language, software side.** Nothing mechanical — no radio, no circuitry,
no enclosure. And no jargon a non-technical judge would stall on: the ideas are
named in ordinary words ("draw a line through the last twenty-four readings and
measure how steep it is") rather than in terms that need translating.

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

---

# ALTERNATIVE — Field Forecast as one continuous block

Use this instead of Beats 1–6 if you'd rather narrate the forecast page in a
single unbroken take. Runs about 1:15. Show the page, press `Simulate storm`,
then read straight through while slowly panning across the three cards.

> This is the same system looking forward instead of back — one card per field.
>
> In a real deployment, this is what a farmer opens in the morning. The top row
> is the weather service: nearly forty millimetres of rain expected over the
> next twelve hours. That's the number everyone in the province gets. On its
> own, it doesn't tell you what to do about it.
>
> Underneath, every field is worked out separately, because every field starts
> in a different condition. North block sits at 661 — healthy, drains fast — so
> the rain passes through and nothing happens there. Riverside is at 821 and
> drying out, so it just soaks it up.
>
> But canal side is already at 469. It's too wet before the storm even arrives,
> there's water sitting on the sensor, and that block holds water instead of
> shedding it. Same rain as the other two, and it floods in about two hours.
>
> That's the point of the whole thing. Not "it's going to rain" — but which of
> your fields is in trouble, and how long you have to do something about it.

**While reading:** start wide on all three cards, drift right to North block on
"661", across to Riverside on "821", then settle on the canal side card for the
last two paragraphs and stay there through the closing line.
