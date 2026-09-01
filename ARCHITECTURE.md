# Architecture

## Overview

FMS is a two-tier wireless sensor network: sensor nodes report to a single base station, which handles alerting and cloud logging. This is a **point-to-multipoint (star) topology today**, not a routing mesh — that distinction matters and is explained below.

## Node tier

Each sensor node consists of:
- A soil moisture sensor (resistive FC-28/YL-69 or capacitive v1.2 — see [`HARDWARE.md`](HARDWARE.md) for the tradeoff and which one is confirmed in use)
- A water level sensor
- An Arduino-class microcontroller
- An HC-12 wireless serial module (433 MHz) for the uplink to the base station

Each node applies signal processing before transmitting, not just raw ADC values:
- **Median filter** (15 samples) to reject noise and splash artifacts
- **Power-gating** on the moisture sensor's VCC (a digital pin, ~10ms energized per reading) to prevent probe corrosion from continuous power — relevant specifically to resistive sensors, not capacitive ones

## Base station tier

The base station is a separate Arduino running `base_station.ino`, which:
- Listens for readings from multiple nodes (the firmware declares per-node API keys — `API_KEY_NODE_2`, `API_KEY_NODE_3` — confirming multi-node support is already architected, not single-node-only)
- Runs the alert decision state machine: `NORMAL → WATCHING → ALERTED → (back to NORMAL once below reset threshold)`
- On a sustained alert condition, sends an SMS via a SIM800L/A7670 GSM module with a Google Maps link to the provisioned location
- Forwards every reading via HTTP GET to a cloud endpoint (currently ThingSpeak, moving to a custom dashboard) with the query contract: `api_key`, `field1=soil`, `field2=water`, `field3=healthIndex`, `lat`, `long`

## Why "point-to-multipoint," not "mesh," today

A true mesh network means nodes can relay each other's traffic, so the network's reach exceeds any single node's radio range and survives individual node failures by rerouting. What's built and bench-tested right now is nodes talking directly to one base station over HC-12 — a star topology. That's a legitimate, useful architecture (it's simpler to debug and sufficient for a small deployment), but it is not multi-hop mesh routing, and this project doesn't claim it is. Multi-hop relay is on the roadmap (see [`ROADMAP.md`](ROADMAP.md)) as a real next step, not a shipped capability.

## Location handling

Coordinates are a **hardcoded constant per unit**, provisioned at install time — not a live GPS read. This was a deliberate call: a flood sensor is installed at a fixed address, so there's no need for it to relocate itself, and removing live GPS removes an entire class of failure mode (GPS lock time, cold-start delay, antenna placement) from the alert path, which matters when the alert path is the one thing that has to work.

**Framing note for competitions and writeups:** because location is provisioned rather than measured, alerts should be described as "location-provisioned at install," not "GPS-tagged" — the latter implies live positioning that isn't there, and judges ask.

## Communication paths, summarized

| Link | Technology | Status |
|---|---|---|
| Sensor node → Base station | HC-12 (433 MHz wireless serial) | Modules soldered, in bench testing/review |
| Base station → Alert recipient | SMS via SIM800L/A7670, AT command set | In active bring-up (see `firmware/sim_at_console/`) |
| Base station → Cloud dashboard | HTTP GET, ThingSpeak-compatible query contract | Working (ThingSpeak); custom dashboard in progress |

## What a full deployment needs that isn't built yet

- Multi-hop relay (true mesh) for coverage beyond single-hop HC-12 range
- Solar charging circuit and battery sizing validated in the field, not just on the bench
- An enclosure rated for the actual deployment environment (flood-adjacent, outdoor, humid)
- A field-tested, non-Rhys installation/maintenance procedure (see the "survives a posting change" goal in `ROADMAP.md`)
