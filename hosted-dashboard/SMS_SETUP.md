# SMS flood alerts: setup

The code is finished. What is left is accounts, secrets and your list.

## How it behaves
A sensor uploads a reading to `/update`. If its last 3 readings are all at WARNING or FLOOD risk, everyone subscribed to that node's zone gets one text (max one per zone per hour). A critically low, discharging battery texts only `ADMIN_NUMBERS` (max one per node per 12 h). Zone = node id.

## 1. PhilSMS account
1. Sign up at https://app.philsms.com and top up credit.
2. Developers > create an API token. This is `PHILSMS_TOKEN`.
3. Register a sender name and **wait for approval**. Philippine carriers reject unapproved names. This is `PHILSMS_SENDER_ID`.

## 2. Secrets (hosting environment variables, never the repo)
| Name | Value |
|---|---|
| `PHILSMS_TOKEN` | from step 1 |
| `PHILSMS_SENDER_ID` | approved sender name |
| `ADMIN_NUMBERS` | your own number(s), comma-separated, e.g. `09171234567` |
| `SMS_ADMIN_KEY` | a long random string: `openssl rand -hex 24` |
| `SMS_INBOUND_KEY` | another long random string |
| `SITE_NAME_1..3` | optional, e.g. `Brgy. San Roque canal` (text under ~25 chars) |
| `SITE_LAT_LONG` | optional fallback, `14.5333,121.0000` |

Publishing applies the database migrations (including the SMS tables).

## 3. Test before real people are involved
```bash
curl -X POST "https://YOUR-DASHBOARD/sms-test?key=$SMS_ADMIN_KEY"
```
You should get a text on `ADMIN_NUMBERS`. If not, the JSON reply says why.

## 4. Load subscribers
Put numbers in `subscribers-zone2.csv` (one per line; git ignores `subscribers*.csv`):
```bash
SMS_ADMIN_KEY=... node scripts/import-subscribers.mjs https://YOUR-DASHBOARD subscribers-zone2.csv --zone 2 --source "sign-up sheet 14 Nov" --consent
```
`--consent` is your confirmation that these people agreed to receive alerts (Data Privacy Act). Re-running is safe; opted-out people are never re-enabled. A `zone,number` CSV loads several zones at once.

## Known gaps
- **STOP replies**: PhilSMS documents no inbound webhook, so replies cannot reach `/sms-inbound` yet. Leave `SMS_OPT_OUT` unset, and mark people stopped by hand (`UPDATE subscribers SET status='stopped' WHERE msisdn='+63...'`) until PhilSMS can forward replies.
- Delivery status is not polled; rows are `sent` or `failed` at send time.
- The rules live in `lib/alerts.ts`; wording in `lib/sms-templates.ts` (mirrors the firmware).
