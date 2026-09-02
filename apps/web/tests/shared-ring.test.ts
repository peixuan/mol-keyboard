import assert from "node:assert/strict";
import test from "node:test";
import {
  SHARED_RING_EVENT_WORDS,
  SHARED_RING_HEADER_WORDS,
  SharedCommandRing,
  SharedEventType,
} from "../src/shared-ring.ts";

test("the shared command ring publishes fixed-width events", () => {
  const capacity = 4;
  const buffer = new SharedArrayBuffer(
    (SHARED_RING_HEADER_WORDS + capacity * SHARED_RING_EVENT_WORDS) * 4,
  );
  const ring = new SharedCommandRing(buffer, capacity);
  assert.equal(ring.noteOn(60, 0.75, 7), true);
  const words = new Int32Array(buffer);
  const floats = new Float32Array(buffer);
  assert.equal(Atomics.load(words, 0), 1);
  assert.equal(words[SHARED_RING_HEADER_WORDS], SharedEventType.NoteOn);
  assert.equal(words[SHARED_RING_HEADER_WORDS + 1], 60);
  assert.equal(floats[SHARED_RING_HEADER_WORDS + 2], 0.75);
  assert.equal(words[SHARED_RING_HEADER_WORDS + 3], 7);
});

test("a full shared ring drops without overwriting unread commands", () => {
  const capacity = 3;
  const buffer = new SharedArrayBuffer(
    (SHARED_RING_HEADER_WORDS + capacity * SHARED_RING_EVENT_WORDS) * 4,
  );
  const ring = new SharedCommandRing(buffer, capacity);
  assert.equal(ring.noteOff(1), true);
  assert.equal(ring.noteOff(2), true);
  assert.equal(ring.noteOff(3), false);
  assert.equal(ring.droppedCount, 1);
  assert.throws(() => new SharedCommandRing(buffer, 4));
});
