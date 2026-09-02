import assert from "node:assert/strict";
import test from "node:test";
import {
  BINDING_BY_CODE,
  FIRST_NOTE,
  KEY_BINDINGS,
  KEY_RECTANGLES,
  KEYBOARD_HEIGHT,
  KEYBOARD_WIDTH,
  LAST_NOTE,
  noteAtPoint,
  noteName,
} from "../src/keyboard.ts";

test("the physical layout covers exactly thirty consecutive notes", () => {
  assert.equal(KEY_BINDINGS.length, 30);
  assert.equal(KEY_BINDINGS[0]?.note, FIRST_NOTE);
  assert.equal(KEY_BINDINGS.at(-1)?.note, LAST_NOTE);
  assert.equal(new Set(KEY_BINDINGS.map(({ code }) => code)).size, 30);
  assert.equal(new Set(KEY_BINDINGS.map(({ note }) => note)).size, 30);
  assert.equal(BINDING_BY_CODE.get("KeyZ")?.note, 60);
  assert.equal(BINDING_BY_CODE.get("BracketLeft")?.note, 89);
});

test("keyboard geometry is bounded and preserves black-key hit priority", () => {
  assert.equal(KEY_RECTANGLES.length, 30);
  for (const key of KEY_RECTANGLES) {
    assert.ok(key.x >= 0);
    assert.ok(key.x + key.width <= KEYBOARD_WIDTH);
    assert.ok(key.y >= 0);
    assert.ok(key.y + key.height <= KEYBOARD_HEIGHT);
  }

  const cSharp = KEY_RECTANGLES.find(({ note }) => note === 61);
  assert.ok(cSharp);
  assert.equal(noteAtPoint(cSharp.x + cSharp.width / 2, 30), 61);
  assert.equal(noteAtPoint(15, KEYBOARD_HEIGHT - 10), 60);
  assert.equal(noteAtPoint(-1, 20), undefined);
});

test("note names use scientific pitch notation", () => {
  assert.equal(noteName(60), "C4");
  assert.equal(noteName(61), "C♯4");
  assert.equal(noteName(89), "F6");
});
