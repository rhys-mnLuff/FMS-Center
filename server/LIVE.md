# Getting live cellular readings onto the dashboard

Status as of 2026-11-05: **no cellular reading has ever arrived.** The modem
registers on the network and then fails on the first transmit. The 26 readings
in `fms.db` all came over USB via `serial_bridge.py` on 23 October.

---

## 1. The blocker is power, and no software change fixes it

The base station boots, finds the SIM, and registers — `AT+CPIN: READY`,
`+CSQ: 13–17/31`, `+CREG: 1`. Then the first `AT+HTTPACTION=0` kills it.

That pattern is a supply collapse, not a firmware fault. The modem draws
roughly **2A in bursts** when the transmitter fires. A laptop USB port supplies
**0.5A (USB 2.0) or 0.9A (USB 3.0)**. The voltage drops the moment the PA keys
up, the board browns out, and the upload never completes.

`HARDWARE.md` already records this, twice:

> Needs its own 3.7–4.2V supply with a large (~1000µF) capacitor — pulling
> power from the Arduino's 5V pin causes brownouts under the module's burst
> current draw, **which looks exactly like a firmware bug**

> Resets or LED dies during send → Power brownout (the classic) → Own supply,
> 1000µF cap; never the Arduino 5V pin

**What to change before testing again:**

1. Power the modem from its **own** 3.7–4.2V source — a 18650 cell, a LiPo, or
   a bench supply set to 4.0V. Not the Arduino's 5V pin, and not USB.
2. Put a **1000µF electrolytic across the modem's VCC and GND**, physically
   close to the module. This is what actually survives the burst; the supply's
   average current rating is not the whole problem.
3. **Common ground** between that supply and the Arduino, or the serial link
   between them will not work.
4. Keep USB connected for the serial log only.

If it still dies after that, the next suspect is the supply's own sag under
load — test with a meter on the modem's VCC during a send, not at the supply.

---

## 2. The second blocker: the endpoint does not survive a restart

`Base_Code.ino` has the server hostname compiled in:

```c
const char SERVER_HOST[] = "generations-unix-come-cartridges.trycloudflare.com";
```

That is a Cloudflare **quick tunnel**. Quick tunnels get a new random hostname
every launch and the old one is gone for good. That hostname has been dead
since the tunnel was last stopped. So even with power fixed, the system cannot
"continuously update" anything: each restart needs a source edit, a recompile
and a reflash.

Two ways out.

### Short term — `server/go_live.sh`

Does the whole loop in one command: starts the ingest server, opens a tunnel,
reads the new hostname, verifies it reaches the server from outside, patches
`SERVER_HOST`, recompiles, flashes, then waits for the first reading.

```bash
./server/go_live.sh                 # full run
./server/go_live.sh --no-flash      # just the tunnel + patch
./server/go_live.sh --port /dev/cu.usbserial-1130
```

This makes the tunnel *usable*, not permanent. It still dies with the laptop.

### Long term — a host that stays up

The firmware points at one hostname forever, the laptop stops being part of the
system, and nothing needs reflashing again.

Checked 2026-11-05, and this has got worse rather than better: **there is no
longer a free tier that is both always-on and plain-HTTP.**

| Option | Cost | Verdict |
|---|---|---|
| Oracle Cloud Always Free VM | free, card required at signup | Real always-on VM, plain HTTP. Best free option |
| Small VPS (Hetzner CX22 etc.) | ~$4–5/mo | Simplest and most predictable |
| Render free web service | free | **Will not work.** Sleeps after 15 min idle, ~60s to wake — the modem gives up after 10s |
| Cloudflare Workers | free, 100k req/day | Always on and well within quota, but HTTPS-only and not Python — would mean rewriting the server and proving TLS on the modem first |
| ThingSpeak | free tier | Works today, but you ruled it out |

Whichever you pick, deploying is: copy `server/fms_server.py`, run it, point
`SERVER_HOST` at the hostname once.

### About HTTPS

Worth correcting an earlier claim: the ATmega328P does **not** need a TLS
stack. The modem terminates TLS itself, so HTTPS is possible in principle —
`AT+HTTPPARA="URL","https://..."` with an SSL context set via `AT+CSSLCFG`.

But SIMCom HTTPS is firmware-revision dependent and a known source of trouble
(errors 702/713 appear repeatedly in the field reports). Do not design around
it until it has been proven on this specific module. Plain HTTP to a host you
control is the lower-risk path, and the payload carries no secrets beyond the
node's write key.

---

## 3. What runs where

One origin serves both the API and the dashboard, so one tunnel — or one
host — exposes the whole system:

```
  /update          ingest, exactly the query string the firmware sends
  /feed            readings as JSON, newest first
  /health          liveness
  /dev.html        the console
  /forecast.html   the per-field forecast
```

The page defaults its feed to **the same origin it was served from**, so a
deployed copy needs no edit. Override with `?feed=<url>` to point a local page
at a deployed server.

---

## 4. Before the data is worth showing

Power and hosting get readings flowing. These decide whether they mean
anything.

- **The last real readings were a fault condition.** Soil 1018 is above
  `RISK_SOIL_FAULT_FROM` (960) — that is the firmware's own "probe is not in
  the ground" band. It was a bench test with the probe in air. Nothing about a
  live feed is useful until the probe is in soil.
- **That node's battery was at 8%** (3660 mV, −78 mA discharging). It will not
  run continuously. Charge or replace before a long run.
- **Nodes 2 and 3 cannot upload at all** — `API_KEY_NODE_2` and `_3` are still
  `REPLACE_WITH_NODE_2_KEY`. The server rejects unknown keys with 401, so those
  nodes will log `FAILED (HTTP 401)` forever.
- **`API_KEY_NODE_1` is committed to a public repo.** Anyone can post fake
  readings. Rotate it and change it in both `Base_Code.ino` and `NODE_KEYS`.
- **A failed upload is dropped, not retried.** A weak-signal window leaves a
  hole in the series, and the dashboard's rate calculation draws straight
  across it.

---

## 5. Order

1. Own supply + 1000µF cap on the modem — **nothing else can be tested first**
2. `./server/go_live.sh` — prove one reading arrives over cellular
3. Probe in actual soil, battery charged — prove the readings mean something
4. Pick a host, point `SERVER_HOST` at it once, stop reflashing
5. Rotate the node 1 key; issue real keys for nodes 2 and 3
6. Upload retry in firmware (2.10) and per-node SMS cooldown (2.4)
