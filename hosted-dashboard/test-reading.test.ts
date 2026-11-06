import { test } from "node:test";
import assert from "node:assert/strict";
import { parseReading, matchesKey } from "./lib/reading.ts";
test("real sensor values preserve signed current and sample age", () => {
  const row = parseReading(new URLSearchParams("field1=766&field2=7&field5=20&field6=3728&field7=-23&age_ms=30000&sample_id=boot:1"), 100000);
  assert.equal(row.batt_ma, -23); assert.equal(row.ts, 70000); assert.equal(row.received_ts, 100000);
  assert.equal(row.rain, null); assert.equal(row.sample_id, "boot:1");
});
test("reject malformed or impossible sensor data", () => {
  for (const q of ["field1=1024&field2=0", "field1=5", "field1=5.1&field2=2", "field1=5&field2=2&field5=101", "field1=5&field2=2&lat=Infinity", "field1=5&field2=2&age_ms=-1"]) {
    assert.throws(() => parseReading(new URLSearchParams(q)));
  }
});
test("missing keys and placeholders cannot authenticate", () => {
  assert.equal(matchesKey("", undefined), false);
  assert.equal(matchesKey("REPLACE_WITH_NODE_2_KEY", "REPLACE_WITH_NODE_2_KEY"), false);
  const key = "a-local-test-secret-at-least-24-characters";
  assert.equal(matchesKey(key, key), true); assert.equal(matchesKey(key.slice(1), key), false);
});
