import assert from "node:assert/strict";
import test from "node:test";

Object.defineProperty(globalThis, "navigator", {
  configurable: true,
  value: { language: "en-US" },
});

const { DEFAULT_SETTINGS, validateSettings } = await import("../src/persistence.ts");

test("default settings satisfy the strict persisted schema", () => {
  assert.equal(validateSettings(DEFAULT_SETTINGS), true);
});

test("unknown, truncated, and out-of-range settings are rejected", () => {
  assert.equal(validateSettings({ ...DEFAULT_SETTINGS, unknown: true }), false);
  assert.equal(validateSettings({ version: 1 }), false);
  assert.equal(validateSettings({ ...DEFAULT_SETTINGS, volume: 1.01 }), false);
  assert.equal(validateSettings({ ...DEFAULT_SETTINGS, preset: 18 }), false);
  assert.equal(validateSettings({ ...DEFAULT_SETTINGS, timeSignature: "7/8" }), false);
});
