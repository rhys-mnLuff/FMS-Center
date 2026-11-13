import { env } from "cloudflare:workers";
import { decideBattery, decideFlood, type Decision, type RecentReading, type Site } from "./alerts";
import { sendSms } from "./philsms";
import { normalisePhNumber, recipientsForZone, type Subscriber } from "./subscribers";
import { withOptOut } from "./sms-templates";

const siteFor = (node: number): Site => ({
  name: (env as unknown as Record<string, string | undefined>)[`SITE_NAME_${node}`] ?? `Node ${node}`,
  latLong: env.SITE_LAT_LONG ?? "",
});

async function lastSent(node: number, kind: string): Promise<number | null> {
  const row = await env.DB!.prepare("SELECT MAX(created_ts) AS t FROM broadcasts WHERE zone=? AND kind=?")
    .bind(node, kind).first<{ t: number | null }>();
  return row?.t ?? null;
}

async function recipients(kind: "flood" | "battery", node: number): Promise<string[]> {
  if (kind === "battery") {
    // Admins only: a farmer cannot fix a flat panel.
    return (env.ADMIN_NUMBERS ?? "").split(",").map(n => normalisePhNumber(n.trim())).filter((n): n is string => !!n);
  }
  // consent_ts IS NOT NULL: no documented consent, no text (Data Privacy Act).
  const { results } = await env.DB!.prepare(
    "SELECT id,zone,msisdn,status FROM subscribers WHERE zone=? AND status='active' AND consent_ts IS NOT NULL",
  ).bind(node).all<Subscriber>();
  return recipientsForZone(results, node);
}

async function dispatch(node: number, d: Extract<Decision, { send: true }>, readingId: number | null, now: number) {
  const to = await recipients(d.kind, node);
  if (!to.length) return;
  const body = d.kind === "flood" && env.SMS_OPT_OUT === "1" ? withOptOut(d.body) : d.body;

  // Claim the cooldown BEFORE sending so two readings landing together cannot both fire.
  const b = await env.DB!.prepare(
    "INSERT INTO broadcasts (zone,kind,body,reading_id,recipient_count,created_ts) VALUES (?,?,?,?,?,?)",
  ).bind(node, d.kind, body, readingId, to.length, now).run();
  const broadcastId = b.meta.last_row_id as number;

  const results = await sendSms({ token: env.PHILSMS_TOKEN!, senderId: env.PHILSMS_SENDER_ID! }, to, body);

  const inserts = results.flatMap(r => r.recipients.map(msisdn =>
    env.DB!.prepare("INSERT INTO sms_sends (broadcast_id,msisdn,provider_id,status,error,updated_ts) VALUES (?,?,?,?,?,?)")
      .bind(broadcastId, msisdn, r.uid ?? null, r.ok ? "sent" : "failed", r.error ?? null, Date.now())));
  await env.DB!.batch(inserts);

  // Like the firmware: a send that failed outright must not start the cooldown.
  if (!results.some(r => r.ok)) {
    await env.DB!.prepare("UPDATE broadcasts SET created_ts=0 WHERE id=?").bind(broadcastId).run();
    console.error("sms: every batch failed for zone", node, results[0]?.error);
  }
}

/** Called after a reading is stored. Never throws: ingest must not depend on SMS. */
export async function runAlerts(node: number, now = Date.now()): Promise<void> {
  try {
    if (!env.DB || !env.PHILSMS_TOKEN || !env.PHILSMS_SENDER_ID) return;
    const { results } = await env.DB.prepare(
      "SELECT id,soil,water,rain,risk,batt_pct,batt_ma,lat,lon FROM readings WHERE node=? ORDER BY ts DESC LIMIT 3",
    ).bind(node).all<RecentReading & { id: number }>();
    if (!results.length) return;
    const site = siteFor(node);

    const flood = decideFlood(results, await lastSent(node, "flood"), now, site);
    if (flood.send) await dispatch(node, flood, results[0].id, now);

    const battery = decideBattery(results[0], await lastSent(node, "battery"), now, site);
    if (battery.send) await dispatch(node, battery, results[0].id, now);
  } catch (e) {
    console.error("sms: runAlerts failed", e);
  }
}
