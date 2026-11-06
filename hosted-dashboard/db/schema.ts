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
