# Cellular dashboard path

Updated 5 November 2026 (Manila).

## Longer-run failure and latest diagnosis

The short wall-power test passed, but subsequent checks found the feed stopped
at **21 readings**, last received **21:58:10 Manila time**. Do not treat the
short run as proof of reliable unattended operation.

On reconnecting the base to USB, it reported **zero radio packets**, including
zero malformed packets, while the modem was registered at **16/31 signal**.
The identified firmware's `t` command successfully reached the hosted `/health`
endpoint over HTTPS without writing a sensor reading. Thus cellular and the
server worked during this diagnostic, while the base heard no sensor node.
The next check is restarting/inspecting the sensor and its radio/sleep behavior.
No new firmware was flashed during this diagnostic.

## Hosted standalone trial (later on 5 November)

The dashboard and a persistent D1 database are published at
https://fms-field-console.bakedhazelnut.chatgpt.site through the Sites plugin.
Anonymous reads and key-protected ingestion are enabled. A new secret Node 1
write key is configured in the hosting environment and in the ignored local
`FmsDeployment.h`; it is not part of the website source. Nodes 2/3 still need
separate provisioned keys before they can upload.

The base currently has the repository's web-upload firmware installed,
configured for HTTPS, TLS 1.2, SNI and validation against Google's GTS Root R4.
The public CA certificate is embedded in program flash and downloaded to the
modem automatically, without the laptop. Certificate validation is enabled;
valid modem time is required. The configured HTTPS path has now delivered real data to the hosted database.
On the wall-power test, USB disappeared from the laptop and the base generated
a new session ID (`1815153`). The first record from that boot arrived at
21:50:28 Manila time after a startup/recovery delay of roughly three minutes.
At least 10 records arrived over the following 367 seconds, with most gaps
around 31 seconds and the longest observed gap 83 seconds. The laptop USB bridge
was stopped, and the hosted database is independent of the local server.

This passes the observed standalone connectivity check, not a guarantee of
uninterrupted cellular coverage or calibrated flood measurements. Keep the base
on its 5V USB wall charger, the SIM/data plan active, and sensor nodes powered.
The soil probe is still reporting in its fault/out-of-soil band. Battery readings
and all physical thresholds still need calibration before field reliance.

This build has SMS disabled and does not include the original Nov firmware's
calls, clock display, saved state or sync-beacon implementation. The original
flash backup remains available. Firmware uses stable sample IDs for retry
deduplication and sends the age of the base's last radio reading. The hosted
server retains both estimated reading time and receipt time. The estimate
includes radio-to-base age; it is not a synchronized clock inside the sensor.

The dashboard starts in live mode. It uses same-origin `/feed`; `?mode=demo`
explicitly selects simulation. Live mode hides simulation controls, shows
missing battery/rain/probe values as unavailable, and does not invent cellular
signal strength, upload percentages or SMS delivery receipts. Timestamps keep
aging even when no new records arrive. Sensor calibration remains provisional.

Local tests passed for authorization rejection, invalid readings, signed current,
record storage, duplicate retries and reading age. The hosted `/health` and
empty `/feed` respond successfully with the `FMS-Base/1.0` client identifier.
Generic Python user-agent requests were rejected at the hosting edge, so device
HTTP setup includes an explicit FMS user agent. Production has no synthetic
sensor records.

Build the configured firmware by copying `Base_Code.ino` and the private header
into an Arduino sketch directory and compiling with `arduino:avr:nano` and
`--build-property compiler.cpp.extra_flags=-DFMS_DEPLOYMENT_CONFIG`. Do not use
the legacy flashing scripts for this configured HTTPS deployment.

## What has actually been proved

The connected base uses an ATmega328P Nano and a modem identifying itself as
**A7670G-LABE**, firmware **A110B06A7670M7**. The older references to SIM7600
in the sketch are variable names and historical assumptions, not identification
of the tested modem.

The installed Nov 2026 firmware announced `ThingSpeak OFF`. Its binary backup
has HTTPTERM but no HTTP uploader commands. The editable repository sketch is a
different version. Do not assume flashing it preserves the Nov firmware's
calls, network clock, saved state or radio sync beacon.

With explicit approval, a temporary build of the repository sketch was tested
against a public Cloudflare tunnel to this ingest server. Outgoing SMS was
suppressed in that build, and the USB serial bridge was stopped throughout.

The corrected build delivered **two real Node 1 readings**, stored as database
rows **56 and 57**, approximately 31 seconds apart. It also completed a cellular
GET `/health`. These are cellular deliveries, not USB bridge deliveries or
synthetic sensor packets. A later upload stalled, so these successes establish
viability, not continuous field reliability. Stage logging then captured modem error **713 (DNS lookup failed)** and
**714 (socket connection failed)**. An explicit, nonpersistent DNS test was
inconclusive because the modem stopped replying during APN setup. A subsequent
recovery test continued hearing sensor packets but did not deliver more uploads.

USB currently supplies power. No supply voltage was measured under load.
Intermittent lost responses and startup messages do not by themselves establish
a power fault. Do not prescribe a voltage for an unidentified carrier board:
its power input may differ from the bare modem's VBAT requirements.

## Firmware changes

`Base_Code.ino` now waits for acknowledgments of APN setup, HTTPINIT and URL
configuration instead of blindly waiting 0.5 seconds for HTTP initialization.
The A76XX manual allows HTTPINIT up to 120 seconds and APN configuration up to
9 seconds. See [SIMCom A76XX command manual](https://files.waveshare.com/wiki/A7670E-Cat-1-GNSS-HAT/A76XX_Series_AT_Command_Manual_V1.09.pdf). HTTPACTION is asynchronous;
the sketch waits for its result before ending the HTTP session.

Setup timeouts now trigger the existing modem recovery path; a one-attempt
baud recovery checks for an answer after changing the boot baud. Failure messages
distinguish DNS and socket errors from a server rejection.

A failed upload retains the latest reading in RAM for another attempt. Attempts
are paced from completion. This is not a durable queue: intermediate readings
may be replaced by newer ones and RAM is lost on reset. Long modem waits also
prevent this SoftwareSerial design from simultaneously receiving radio packets.
The server timestamps receipt, not the sensor's sampling time.

The startup offline timeout now defaults to normal operation. The old 10-second
demo default falsely marked a node offline before its next 30-second packet.
After packets arrive, the existing interval learning still applies.

## How permanent operation will work

Sensor → HC-12 radio → base → cellular data → permanent ingest server/database
→ dashboard polling `/feed`.

A domain is a stable name; it does not supply an always-running server. The
permanent deployment must serve the ingestion API and dashboard, retain the
database, and restart the process after failures. The laptop is only part of
the temporary tunnel test. A tunnel hostname can disappear when the tunnel
stops and must not be treated as a permanent firmware configuration.

Current routes:

- `/update`: authenticated sensor ingestion using per-node write keys
- `/feed`: JSON readings for the dashboard
- `/health`: server liveness and reading freshness
- `/dev.html`: dashboard
- `/forecast.html`: field view

Set `SERVER_HOST` and `SERVER_PATH` to the permanent deployment, then rebuild
and test. The currently tested uploader uses plain HTTP. HTTPS/TLS on this
specific modem revision has not yet been validated. Protect ingestion keys
in deployment and replace the public/placeholder keys before field use.

## Tools and restoration

- `firmware/CellularDiagnostic/CellularDiagnostic.ino`: temporary USB-to-modem
  diagnostic, USB 115200, modem D10/D11 at 9600; no sensor forwarding, SMS or calls.
- `firmware/pre-cellular-test-2026-11-05.hex`: flash backup taken before testing.
- `firmware/invalid-eeprom-dump-2026-11-05.hex`: **NOT an EEPROM backup**.
  The bootloader returned the first flash KB for EEPROM reads. An attempted
  EEPROM restoration failed verification and altered flash; the original flash
  was reapplied afterward. EEPROM settings cannot be independently verified
  from this dump. Do not use this file for restoration.
- `firmware/board-backup-2026-11-05.hex`: earlier original firmware backup.

The tested Nano uses the **115200-baud bootloader** (`arduino:avr:nano`).
The 57600-baud old-bootloader read failed; the 115200 read and uploads succeeded.

`check_cellular.sh` and `go_live.sh` **flash firmware by default**. The former
uses a third-party test host rather than this project's ingest endpoint.
`go_live.sh --no-flash` still changes the source hostname and opens a public
tunnel. Neither script is a passive first diagnostic for an unidentified build.
On the Nov firmware, `t` sends a test SMS; on the repository firmware, it tests
HTTP. Never issue `t` before identifying the running firmware.

## Remaining acceptance checks

Repeated delivery must stay reliable, then recover from network interruptions
and a full power cycle. Field operation needs its own suitable power source so
USB can be disconnected. Probe calibration and placement, battery calibration,
stale-data handling, and a persistent host remain separate requirements.

## End of the earlier temporary test

The original application was restored and **all 32,768 flash bytes verified**
against the pre-test backup. The temporary public tunnel was closed. The local
USB bridge is restored separately; USB deliveries after row 57 are not evidence
of cellular delivery. The two cellular records are preserved in
`test-results/2026-11-05-cellular-readings.json`, with serial logs alongside.

The original firmware used in that earlier restoration has uploads OFF.
The later hosted trial described above subsequently installed the configured
HTTPS uploader, which is the currently running build. The source fixes need further reliability
testing and eventual integration with the desired Nov 2026 features before a
permanent field deployment. Nothing was pushed or deployed permanently.
