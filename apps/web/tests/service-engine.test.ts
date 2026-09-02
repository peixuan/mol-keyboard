import assert from "node:assert/strict";
import test from "node:test";

import { buildServiceWebSocketUrl } from "../src/service-engine.ts";

const TOKEN = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

test("service URLs are restricted to an authenticated loopback control endpoint", () => {
  assert.equal(
    buildServiceWebSocketUrl("ws://127.0.0.1:8766/control", TOKEN),
    `ws://127.0.0.1:8766/control?token=${TOKEN}`,
  );
  assert.equal(
    buildServiceWebSocketUrl("ws://localhost:9000/control", TOKEN),
    `ws://localhost:9000/control?token=${TOKEN}`,
  );
});

test("remote, credentialed, malformed, and pre-tokenized endpoints are rejected", () => {
  for (const endpoint of [
    "wss://example.com/control",
    "ws://192.168.1.5:8766/control",
    "ws://user@127.0.0.1:8766/control",
    "ws://127.0.0.1:8766/other",
    "ws://127.0.0.1:8766/control?token=committed",
  ]) {
    assert.throws(() => buildServiceWebSocketUrl(endpoint, TOKEN));
  }
  assert.throws(() => buildServiceWebSocketUrl("ws://127.0.0.1:8766/control", "short"));
});
