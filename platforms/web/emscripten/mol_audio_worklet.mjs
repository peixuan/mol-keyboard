// SPDX-License-Identifier: Apache-2.0

const molModule = {};
// With synchronous Wasm compilation the factory populates and initializes the
// caller's object before its compatibility Promise resolves. AudioWorklet
// module loading cannot suspend on that Promise, so the processor keeps the
// already-initialized object and intentionally ignores the resolved value.
void Module(molModule);
const initialize = molModule.cwrap("mol_wasm_initialize", "number", [
  "number",
  "number",
  "number",
]);
const noteOn = molModule.cwrap("mol_wasm_note_on", "number", [
  "number",
  "number",
  "number",
]);
const noteOff = molModule.cwrap("mol_wasm_note_off", "number", ["number"]);
const render = molModule.cwrap("mol_wasm_render", "number", ["number", "number"]);

const MOL_RENDER_QUANTUM = 128;
const MOL_CHANNEL_COUNT = 2;
const MOL_MAX_VOICES = 8;
const MOL_MAX_MESSAGE_EVENTS = 64;

function validGestureId(value) {
  return Number.isInteger(value) && value > 0 && value <= 0xffffffff;
}

function validNote(value) {
  return Number.isInteger(value) && value >= 0 && value <= 127;
}

function validVelocity(value) {
  return Number.isFinite(value) && value > 0 && value <= 1;
}

class MolAudioProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.ready = initialize(sampleRate, MOL_CHANNEL_COUNT, MOL_MAX_VOICES) === 0;
    this.activeGestures = new Set();
    const initialNote = options?.processorOptions?.initialNote;
    if (this.ready && initialNote !== undefined) {
      noteOn(initialNote, 1.0, 1);
    }
    this.port.postMessage({ type: "ready", ready: this.ready });
    this.port.onmessage = (event) => {
      const message = event.data;
      if (!this.ready || message === null || typeof message !== "object") {
        return;
      }
      if (message.type === "events") {
        if (!Array.isArray(message.events) || message.events.length > MOL_MAX_MESSAGE_EVENTS) {
          this.port.postMessage({ type: "events-processed", accepted: 0, rejected: 1 });
          return;
        }
        let accepted = 0;
        for (const item of message.events) {
          if (this.handleEvent(item)) accepted += 1;
        }
        this.port.postMessage({
          type: "events-processed",
          accepted,
          rejected: message.events.length - accepted,
        });
      } else {
        this.handleEvent(message);
      }
    };
  }

  handleEvent(message) {
    if (message === null || typeof message !== "object") return false;
    if (message.type === "note-on") {
      if (
        !validNote(message.note) ||
        !validVelocity(message.velocity) ||
        !validGestureId(message.gestureId) ||
        this.activeGestures.has(message.gestureId)
      ) {
        return false;
      }
      if (noteOn(message.note, message.velocity, message.gestureId) !== 0) return false;
      this.activeGestures.add(message.gestureId);
      return true;
    }
    if (message.type === "note-off") {
      if (!validGestureId(message.gestureId) || !this.activeGestures.has(message.gestureId)) {
        return false;
      }
      if (noteOff(message.gestureId) !== 0) return false;
      this.activeGestures.delete(message.gestureId);
      return true;
    }
    if (message.type === "all-notes-off") {
      for (const gestureId of this.activeGestures) noteOff(gestureId);
      this.activeGestures.clear();
      return true;
    }
    return false;
  }

  process(_inputs, outputs) {
    const output = outputs[0];
    if (output === undefined || output.length === 0) {
      return true;
    }

    const frameCount = output[0].length;
    if (!this.ready || frameCount > MOL_RENDER_QUANTUM) {
      for (let channel = 0; channel < output.length; channel += 1) {
        output[channel].fill(0.0);
      }
      return true;
    }

    const pointer = render(frameCount, MOL_CHANNEL_COUNT);
    if (pointer === 0) {
      for (let channel = 0; channel < output.length; channel += 1) {
        output[channel].fill(0.0);
      }
      return true;
    }

    const base = pointer >>> 2;
    for (let frame = 0; frame < frameCount; frame += 1) {
      const left = molModule.HEAPF32[base + frame * MOL_CHANNEL_COUNT];
      output[0][frame] = left;
      if (output.length > 1) {
        output[1][frame] = molModule.HEAPF32[base + frame * MOL_CHANNEL_COUNT + 1];
      }
      for (let channel = MOL_CHANNEL_COUNT; channel < output.length; channel += 1) {
        output[channel][frame] = left;
      }
    }
    return true;
  }
}

registerProcessor("mol-keyboard", MolAudioProcessor);
