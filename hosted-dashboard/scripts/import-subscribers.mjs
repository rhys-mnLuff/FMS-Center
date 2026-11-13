#!/usr/bin/env node
/**
 * Upload a CSV of subscribers to the dashboard.
 *
 *   SMS_ADMIN_KEY=... node scripts/import-subscribers.mjs https://your-dashboard.example numbers.csv
 *
 * CSV: one row per person, either `number` (then pass --zone 2) or `zone,number`.
 * A header row is fine. Numbers like 0917 123 4567 or +639171234567 both work.
 *
 * Passing --consent confirms these people agreed to receive flood alerts.
 * Without it nothing is uploaded.
 *
 * Keep the CSV out of git (subscribers*.csv is ignored) and out of chat.
 */
import { readFileSync } from "node:fs";

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i < 0 ? null : args.splice(i, i + 1 < args.length && !args[i + 1].startsWith("--") ? 2 : 1)[1] ?? true; };
const consent = flag("--consent") === true;
const zoneArg = flag("--zone");
const source = flag("--source") ?? "csv import";
const [host, file] = args;
const key = process.env.SMS_ADMIN_KEY;

if (!host || !file || !key) { console.error("usage: SMS_ADMIN_KEY=... node scripts/import-subscribers.mjs <https://host> <file.csv> [--zone N] [--source text] --consent"); process.exit(1); }
if (!consent) { console.error("Refusing: add --consent to confirm these people agreed to receive flood alerts."); process.exit(1); }

const byZone = new Map();
for (const line of readFileSync(file, "utf8").split(/\r?\n/)) {
  const cells = line.split(",").map(c => c.trim().replace(/^"|"$/g, ""));
  if (!cells[0] || /^(zone|number|phone|mobile)/i.test(cells[0])) continue;
  const [zone, number] = cells.length > 1 ? [Number(cells[0]), cells[1]] : [Number(zoneArg), cells[0]];
  if (!Number.isInteger(zone) || zone < 1) { console.error(`No zone for row "${line.slice(0, 4)}…" -- use --zone N or a zone,number CSV`); process.exit(1); }
  (byZone.get(zone) ?? byZone.set(zone, []).get(zone)).push(number);
}

for (const [zone, numbers] of byZone) {
  const res = await fetch(`${host.replace(/\/$/, "")}/sms-subscribers?key=${encodeURIComponent(key)}`, {
    method: "POST", headers: { "content-type": "application/json" },
    body: JSON.stringify({ zone, consent: true, source, numbers }),
  });
  const out = await res.json().catch(() => ({}));
  console.log(`zone ${zone}: HTTP ${res.status}`, res.ok ? `added ${out.added}, already there ${out.already_present}, invalid ${out.invalid_count}` : out);
  if (out.invalid_count) console.log("  invalid rows start with:", out.invalid.join(" "));
}
