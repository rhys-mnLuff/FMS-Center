/**
 * When to text. Pure functions: readings and past broadcasts in, a decision out.
 *
 * Mirrors Base_Code.ino so the server and the base station agree:
 *   flood   -- ALERT_CONFIRM_READINGS (3) consecutive readings at risk >= WARNING,
 *              then at most one per SMS_COOLDOWN_MS (1 h). Per zone here, because
 *              the firmware's cooldown is global but recipients are per zone.
 *   battery -- critical and discharging, at most one per 12 h per node.
 */
import { buildFloodSms, buildLowBatterySms } from "./sms-templates";

export const ALERT_CONFIRM_READINGS = 3;
export const FLOOD_COOLDOWN_MS = 3_600_000;
export const BATTERY_COOLDOWN_MS = 43_200_000;
export const BATTERY_CRITICAL_PCT = 15;   // firmware FLAG_BATT_CRITICAL threshold is set on the node; this is the server-side equivalent
const RISK_WARNING = 2;

export interface RecentReading {
  soil: number; water: number; rain: number | null; risk: number | null;
  batt_pct: number | null; batt_ma: number | null;
  lat: number | null; lon: number | null;
}

export interface Site { name: string; latLong: string }

export type Decision = { send: false; reason: string } | { send: true; kind: "flood" | "battery"; body: string };

/** `recent` is newest first. `lastSentTs` is the last broadcast of this kind for this zone. */
export function decideFlood(recent: RecentReading[], lastSentTs: number | null, now: number, site: Site): Decision {
  if (recent.length < ALERT_CONFIRM_READINGS) return { send: false, reason: "not enough readings" };
  const window = recent.slice(0, ALERT_CONFIRM_READINGS);
  if (!window.every(r => (r.risk ?? 0) >= RISK_WARNING)) return { send: false, reason: "below warning or not confirmed" };
  if (lastSentTs !== null && now - lastSentTs < FLOOD_COOLDOWN_MS) return { send: false, reason: "cooldown" };
  const r = window[0];
  const latLong = r.lat !== null && r.lon !== null ? `${r.lat},${r.lon}` : site.latLong;
  const rain = r.rain === 0 || r.rain === 1 || r.rain === 2 ? r.rain : null;
  return { send: true, kind: "flood", body: buildFloodSms({ siteName: site.name, soil: r.soil, water: r.water, rain, latLong }) };
}

export function decideBattery(latest: RecentReading, lastSentTs: number | null, now: number, site: Site): Decision {
  const { batt_pct: pct, batt_ma: ma } = latest;
  if (pct === null || pct > BATTERY_CRITICAL_PCT) return { send: false, reason: "battery ok" };
  if (ma === null || ma >= 0) return { send: false, reason: "not discharging" };
  if (lastSentTs !== null && now - lastSentTs < BATTERY_COOLDOWN_MS) return { send: false, reason: "cooldown" };
  return { send: true, kind: "battery", body: buildLowBatterySms({ siteName: site.name, pct }) };
}
