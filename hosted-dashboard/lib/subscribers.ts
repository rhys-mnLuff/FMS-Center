/**
 * Zone-based recipient selection and opt-out.
 *
 * Each node is its own zone with its own subscriber group: a flood at node 2
 * texts zone 2 and nobody else. Texting all 700 for a one-node event would
 * train people to ignore the alerts, which is the failure mode that matters
 * most here.
 *
 * NO PHONE NUMBERS LIVE IN THIS REPO. Subscribers are rows in D1, loaded at
 * send time. The fixtures in the tests are deliberately invalid-but-shaped
 * placeholders.
 */

/**
 * Philippine mobile numbers to E.164.
 *
 * This is load-bearing for opt-out, not cosmetic. Someone can sign up as
 * 0917 123 4567 and then text STOP from a handset the gateway reports as
 * 639171234567. If those two do not reduce to the same string, we keep
 * texting a person who asked us to stop -- which is the one failure that
 * turns a safety service into harassment.
 *
 * Accepts 09XXXXXXXXX, 9XXXXXXXXX, 639XXXXXXXXX, +639XXXXXXXXX, with any
 * spaces, dashes or brackets. Returns null for anything that is not a PH
 * mobile number, so callers must handle null rather than send blind.
 */
export function normalisePhNumber(raw: string): string | null {
  if (typeof raw !== "string") return null;
  let d = raw.replace(/[^\d+]/g, "");
  if (d.startsWith("+")) d = d.slice(1);
  if (d.startsWith("00")) d = d.slice(2);          // 0063... international prefix
  if (d.startsWith("63")) d = d.slice(2);
  else if (d.startsWith("0")) d = d.slice(1);
  // What is left must be a 10-digit mobile subscriber number starting with 9.
  // PH landlines and short codes are not valid alert destinations.
  if (!/^9\d{9}$/.test(d)) return null;
  return "+63" + d;
}

export type SubscriberStatus = "active" | "stopped";

export interface Subscriber {
  id: number;
  zone: number;            // node id this person is subscribed to
  msisdn: string;          // E.164, as returned by normalisePhNumber
  status: SubscriberStatus;
}

/**
 * Who gets texted for an event at `zone`.
 *
 * Three rules, in this order:
 *   1. only this zone -- that is the whole point of the request
 *   2. never anyone stopped
 *   3. one message per number, even if the same person appears twice
 *
 * Rule 3 matters because a number can legitimately be in two zones (a farmer
 * with land either side of the canal). For a single-zone event that is not a
 * duplicate, but the dedupe costs nothing and protects the multi-zone case.
 */
export function recipientsForZone(subs: Subscriber[], zone: number): string[] {
  const seen = new Set<string>();
  for (const s of subs) {
    if (s.zone !== zone) continue;
    if (s.status !== "active") continue;
    const n = normalisePhNumber(s.msisdn);
    if (!n) continue;                 // malformed row: skip, never send blind
    seen.add(n);
  }
  return [...seen];
}

/**
 * STOP applies to the person, not the zone.
 *
 * Someone subscribed to zones 1 and 3 who texts STOP means "stop texting me",
 * not "stop texting me about zone 1". Returns the numbers to mark stopped;
 * the caller updates every row holding that msisdn.
 */
export function stopMatchesFor(subs: Subscriber[], from: string): number[] {
  const n = normalisePhNumber(from);
  if (!n) return [];
  return subs
    .filter(s => normalisePhNumber(s.msisdn) === n && s.status === "active")
    .map(s => s.id);
}

/**
 * Does an inbound message mean STOP?
 *
 * No Philippine provider does this for us -- there is no carrier-level STOP
 * keyword as in the US -- so the keyword list is ours. Kept deliberately
 * loose: a person trying to leave should not have to guess the magic word,
 * and the cost of a false positive (one person stops getting alerts they can
 * re-subscribe to) is far lower than a false negative.
 */
const STOP_WORDS = ["stop", "unsub", "unsubscribe", "cancel", "end", "quit", "opt out", "optout"];
export function isStopMessage(body: string): boolean {
  if (typeof body !== "string") return false;
  const t = body.trim().toLowerCase().replace(/[.!,]+$/, "");
  return STOP_WORDS.some(w => t === w || t.startsWith(w + " "));
}
