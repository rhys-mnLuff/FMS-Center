/**
 * SMS wording, ported verbatim from Base_Code.ino.
 *
 * The base station texts 2-4 admin numbers itself. The 700-person list is
 * sent from here. Both must read identically -- an admin comparing their
 * handset against a farmer's should see the same words, or neither trusts
 * the system.
 *
 * Every string below is copied from the firmware's PSTR() literals. The
 * firmware is the source of truth: if a template changes there, change it
 * here and update sms-templates.test.ts, which pins the exact output.
 *
 * Thresholds mirror the firmware constants of the same name. Duplicated
 * rather than imported because nothing on this side parses C -- the test
 * is what keeps them honest.
 */

export const BRIEF_WATER_ALERT_ABOVE = 30;      // water > 30 = standing water
export const BRIEF_SOIL_ALERT_AT_OR_BELOW = 436; // soil <= 436 = saturated
export const RISK_SOIL_WATCH_BELOW = 552;
export const RISK_SOIL_FAULT_FROM = 960;         // at/above = probe out of soil

/** Base_Code.ino soilWords() */
export function soilWords(soil: number): string {
  if (soil >= RISK_SOIL_FAULT_FROM) return "no reading";
  if (soil <= BRIEF_SOIL_ALERT_AT_OR_BELOW) return "SATURATED";
  if (soil < RISK_SOIL_WATCH_BELOW) return "wet";
  if (soil < 700) return "normal";
  if (soil < 829) return "dry";
  return "very dry";
}

/** Base_Code.ino rainWords(). `rain` is the 2-bit FLAG_RAIN_MASK value. */
export function rainWords(rain: 0 | 1 | 2 | 3 | null): string {
  switch (rain) {
    case 0: return "no rain";
    case 1: return "light rain";
    case 2: return "heavy rain";
    default: return "no reading";
  }
}

/** Base_Code.ino riskWords() */
export function riskWords(level: number): string {
  switch (level) {
    case 3: return "FLOODING NOW";
    case 2: return "WARNING - flood possible";
    case 1: return "WATCH - keep an eye out";
    default: return "NORMAL";
  }
}

export interface FloodSmsInput {
  siteName: string;        // under ~25 chars, as SITE_NAME_n in the firmware
  soil: number;
  water: number;
  rain: 0 | 1 | 2 | 3 | null;
  latLong: string;         // "14.5333,121.0000" -- SITE_LAT_LONG
  includeRaw?: boolean;    // firmware appends this only if it fits in the SMS
}

/**
 * Base_Code.ino buildFloodSms().
 *
 * No link, deliberately. SMS_MAP_LINK is 0 in the firmware because Philippine
 * networks were observed rejecting texts containing links (tested 16 Oct 2026
 * on Smart), so the location goes out as bare coordinates. Keep it that way:
 * a link here would be both inconsistent with the base and undeliverable.
 */
export function buildFloodSms(i: FloodSmsInput): string {
  const waterThere = i.water > BRIEF_WATER_ALERT_ABOVE;
  let s = "";
  s += waterThere ? "FMS FLOOD ALERT\n" : "FMS FLOOD WARNING\n";
  s += `${i.siteName}\n`;
  s += `Water: ${waterThere ? "FLOODING" : "none yet"}\n`;
  s += `Soil: ${soilWords(i.soil)}\n`;
  s += `Rain: ${rainWords(i.rain)}\n`;
  s += waterThere ? "Act now: move to higher ground\n" : "Flood likely soon. Get ready\n";
  s += `Loc: ${i.latLong}`;
  if (i.includeRaw) s += `\nRaw s${i.soil} w${i.water}`;
  return s;
}

export interface LowBatterySmsInput { siteName: string; pct: number }

/** Base_Code.ino buildLowBatterySms() */
export function buildLowBatterySms(i: LowBatterySmsInput): string {
  return `FMS BATTERY LOW\n${i.siteName}\nBattery: ${i.pct}% and draining\n`
    + "Sensor may stop soon. Check the solar panel is clean and in sun";
}

/**
 * Not in the firmware. The base has no opt-out because it only texts admins
 * who were provisioned by hand; a 700-person list does need one, and nothing
 * in the Philippines adds it for you -- there is no carrier-level STOP
 * keyword the way there is in the US. So it is appended here, and the inbound
 * webhook has to honour it.
 *
 * Costs characters: at 160 chars a message splits into two billable parts,
 * so this is appended only where there is room.
 */
export const OPT_OUT_SUFFIX = "\nReply STOP to opt out";

/** GSM-03.38 single part is 160 chars; concatenated parts are 153 each. */
export function smsParts(body: string): number {
  return body.length <= 160 ? 1 : Math.ceil(body.length / 153);
}

/** Append the opt-out line only when it does not push into another part. */
export function withOptOut(body: string): string {
  const withSuffix = body + OPT_OUT_SUFFIX;
  return smsParts(withSuffix) === smsParts(body) ? withSuffix : body;
}
