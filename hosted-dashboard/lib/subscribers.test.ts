import { describe, expect, it } from "vitest";
import {
  normalisePhNumber, recipientsForZone, stopMatchesFor, isStopMessage,
  type Subscriber,
} from "./subscribers";

/**
 * Numbers here are shaped like PH mobiles but are placeholders. No real
 * subscriber number belongs in this repo.
 */
const sub = (id: number, zone: number, msisdn: string,
             status: "active" | "stopped" = "active"): Subscriber =>
  ({ id, zone, msisdn, status });

describe("normalisePhNumber", () => {
  it.each([
    ["09171234567",      "+639171234567"],
    ["9171234567",       "+639171234567"],
    ["639171234567",     "+639171234567"],
    ["+639171234567",    "+639171234567"],
    ["+63 917 123 4567", "+639171234567"],
    ["0917-123-4567",    "+639171234567"],
    ["(0917) 123 4567",  "+639171234567"],
    ["0063 917 123 4567","+639171234567"],
  ])("%s -> %s", (raw, want) => expect(normalisePhNumber(raw)).toBe(want));

  it.each([
    ["", "empty"],
    ["0281234567", "landline, not mobile"],
    ["091712345", "too short"],
    ["091712345678", "too long"],
    ["+14155550100", "not a PH number"],
    ["not a number", "garbage"],
    ["8171234567", "does not start with 9"],
  ])("rejects %s (%s)", (raw) => expect(normalisePhNumber(raw)).toBeNull());
});

describe("recipientsForZone keeps a one-node flood inside one zone", () => {
  const subs = [
    sub(1, 1, "09171234567"),
    sub(2, 1, "09181234567"),
    sub(3, 2, "09191234567"),
    sub(4, 3, "09201234567"),
  ];

  it("a flood at node 2 texts only zone 2", () => {
    expect(recipientsForZone(subs, 2)).toEqual(["+639191234567"]);
  });

  it("zone 1 gets both of its own and none of the others", () => {
    expect(recipientsForZone(subs, 1).sort())
      .toEqual(["+639171234567", "+639181234567"]);
  });

  it("an empty zone sends to nobody rather than falling back to everyone", () => {
    expect(recipientsForZone(subs, 9)).toEqual([]);
  });
});

describe("recipientsForZone excludes and dedupes", () => {
  it("never texts someone who stopped", () => {
    const subs = [sub(1, 1, "09171234567", "stopped"), sub(2, 1, "09181234567")];
    expect(recipientsForZone(subs, 1)).toEqual(["+639181234567"]);
  });

  it("one message per person even when the same number is in a zone twice", () => {
    // Same human, two formats -- a hand-entered list will do this.
    const subs = [sub(1, 1, "09171234567"), sub(2, 1, "+63 917 123 4567")];
    expect(recipientsForZone(subs, 1)).toEqual(["+639171234567"]);
  });

  it("skips a malformed row instead of sending to it", () => {
    const subs = [sub(1, 1, "not a number"), sub(2, 1, "09181234567")];
    expect(recipientsForZone(subs, 1)).toEqual(["+639181234567"]);
  });
});

describe("STOP applies to the person, not the zone", () => {
  const subs = [
    sub(1, 1, "09171234567"),
    sub(2, 3, "+639171234567"),   // same person, different zone, different format
    sub(3, 1, "09181234567"),
  ];

  it("stops every row for that number across all zones", () => {
    expect(stopMatchesFor(subs, "639171234567").sort()).toEqual([1, 2]);
  });

  it("matches regardless of the format the handset reports", () => {
    for (const f of ["09171234567", "+639171234567", "639171234567", "+63 917 123 4567"]) {
      expect(stopMatchesFor(subs, f).sort()).toEqual([1, 2]);
    }
  });

  it("does not touch anyone else", () => {
    expect(stopMatchesFor(subs, "09181234567")).toEqual([3]);
  });

  it("ignores an already-stopped row", () => {
    expect(stopMatchesFor([sub(1, 1, "09171234567", "stopped")], "09171234567")).toEqual([]);
  });

  it("an unparseable sender stops nobody", () => {
    expect(stopMatchesFor(subs, "garbage")).toEqual([]);
  });
});

describe("isStopMessage is generous about what counts as leaving", () => {
  it.each(["STOP", "stop", " Stop ", "STOP.", "unsubscribe", "UNSUB", "cancel",
           "quit", "end", "opt out", "stop please"])("%s -> stop", (b) =>
    expect(isStopMessage(b)).toBe(true));

  it.each(["", "status", "stopped the rain", "help", "FMS STATUS", "nonstop"])(
    "%s -> not stop", (b) => expect(isStopMessage(b)).toBe(false));
});
