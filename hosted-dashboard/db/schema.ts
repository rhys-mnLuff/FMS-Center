import { integer, real, sqliteTable, text, index, uniqueIndex } from "drizzle-orm/sqlite-core";
export const readings = sqliteTable("readings", {
  id: integer("id").primaryKey({ autoIncrement: true }),
  node: integer("node").notNull(), ts: integer("ts").notNull(), received_ts: integer("received_ts").notNull(),
  soil: integer("soil").notNull(), water: integer("water").notNull(),
  health: integer("health"), rain: integer("rain"),
  batt_pct: integer("batt_pct"), batt_mv: integer("batt_mv"), batt_ma: integer("batt_ma"),
  risk: integer("risk"), lat: real("lat"), lon: real("lon"),
  sample_id: text("sample_id"),
}, table => [index("readings_node_ts").on(table.node, table.ts),
  index("readings_ts").on(table.ts),
  uniqueIndex("readings_node_sample").on(table.node, table.sample_id)]);

/* ---------------------------------------------------------------------------
   SMS alerting.

   Subscriber numbers live here and NOWHERE in the repo. Zone = node id, so a
   flood at one node texts that zone's group only.

   consent_ts is not decoration: the Data Privacy Act of 2012 requires
   documented consent before sending, so the row has to record when it was
   given. A subscriber with no consent_ts should not be texted.
--------------------------------------------------------------------------- */
export const subscribers = sqliteTable("subscribers", {
  id: integer("id").primaryKey({ autoIncrement: true }),
  zone: integer("zone").notNull(),              // node id this person signed up for
  msisdn: text("msisdn").notNull(),             // E.164, +639XXXXXXXXX
  status: text("status").notNull().default("active"),   // active | stopped
  consent_ts: integer("consent_ts"),            // when they opted in
  stopped_ts: integer("stopped_ts"),            // when they texted STOP
  source: text("source"),                       // how they joined, for audit
  created_ts: integer("created_ts").notNull(),
}, table => [
  uniqueIndex("subscribers_zone_msisdn").on(table.zone, table.msisdn),
  // STOP looks a number up across every zone, so this index is the one that
  // makes opt-out fast rather than a full scan per inbound message.
  index("subscribers_msisdn").on(table.msisdn),
  index("subscribers_zone_status").on(table.zone, table.status),
]);

/* One row per alert, holding the body once rather than 700 times. */
export const broadcasts = sqliteTable("broadcasts", {
  id: integer("id").primaryKey({ autoIncrement: true }),
  zone: integer("zone").notNull(),
  kind: text("kind").notNull().default("flood"),   // flood | battery -- each has its own cooldown
  body: text("body").notNull(),
  reading_id: integer("reading_id"),            // the reading that triggered it
  recipient_count: integer("recipient_count").notNull(),
  created_ts: integer("created_ts").notNull(),
}, table => [index("broadcasts_zone_ts").on(table.zone, table.created_ts)]);

/* Per-recipient delivery state. Semaphore has no delivery webhook (status is polled via GET /messages/{id}), so status
   is updated by polling rather than pushed; the column is the same either
   way, which keeps a later provider switch from touching the schema. */
export const sms_sends = sqliteTable("sms_sends", {
  id: integer("id").primaryKey({ autoIncrement: true }),
  broadcast_id: integer("broadcast_id").notNull(),
  msisdn: text("msisdn").notNull(),
  provider_id: text("provider_id"),             // provider's message id, for status lookup
  status: text("status").notNull().default("queued"),  // queued|sent|delivered|failed
  error: text("error"),
  updated_ts: integer("updated_ts").notNull(),
}, table => [
  index("sms_sends_broadcast").on(table.broadcast_id),
  index("sms_sends_status").on(table.status),
]);
