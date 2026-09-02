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
const submitScalar = molModule.cwrap("mol_wasm_submit_scalar", "number", ["number", "number"]);
const submitInteger = molModule.cwrap("mol_wasm_submit_integer", "number", ["number", "number"]);
const submitParameter = molModule.cwrap("mol_wasm_submit_parameter", "number", [
  "number",
  "number",
]);
const submitPreset = molModule.cwrap("mol_wasm_submit_preset", "number", ["number", "number"]);
const submitScale = molModule.cwrap("mol_wasm_submit_scale", "number", [
  "number",
  "number",
  "number",
]);
const submitArpeggiator = molModule.cwrap("mol_wasm_submit_arpeggiator", "number", [
  "number",
  "number",
  "number",
  "number",
  "number",
]);
const submitTimeSignature = molModule.cwrap("mol_wasm_submit_time_signature", "number", [
  "number",
  "number",
]);
const submitMetronome = molModule.cwrap("mol_wasm_submit_metronome", "number", [
  "number",
  "number",
]);
const submitPortamento = molModule.cwrap("mol_wasm_submit_portamento", "number", [
  "number",
  "number",
]);
const submitAction = molModule.cwrap("mol_wasm_submit_action", "number", ["number"]);
const pollEvents = molModule.cwrap("mol_wasm_poll_events", "number", []);
const eventBuffer = molModule.cwrap("mol_wasm_event_buffer", "number", []);
const render = molModule.cwrap("mol_wasm_render", "number", ["number", "number"]);

const MOL_RENDER_QUANTUM = 128;
const MOL_CHANNEL_COUNT = 2;
const MOL_MAX_VOICES = 8;
const MOL_MAX_MESSAGE_EVENTS = 64;
const MOL_EVENT_WORDS = 4;

const COMMAND = Object.freeze({
  sustain: 5,
  allNotesOff: 6,
  allSoundOff: 7,
  masterGain: 8,
  octave: 11,
  transpose: 12,
  chord: 14,
  tempo: 16,
  transportStart: 18,
  transportStop: 19,
  recordStart: 21,
  recordStop: 22,
  playbackStart: 23,
  playbackStop: 24,
  reset: 26,
});

function validGestureId(value) {
  return Number.isInteger(value) && value > 0 && value <= 0xffffffff;
}

function validNote(value) {
  return Number.isInteger(value) && value >= 0 && value <= 127;
}

function validVelocity(value) {
  return Number.isFinite(value) && value > 0 && value <= 1;
}

function integerInRange(value, minimum, maximum) {
  return Number.isInteger(value) && value >= minimum && value <= maximum;
}

function numberInRange(value, minimum, maximum) {
  return Number.isFinite(value) && value >= minimum && value <= maximum;
}

class MolAudioProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.ready = initialize(sampleRate, MOL_CHANNEL_COUNT, MOL_MAX_VOICES) === 0;
    this.activeGestures = new Set();
    this.engineEventMessage = {
      type: "engine-events",
      count: 0,
      words: new Uint32Array(MOL_MAX_MESSAGE_EVENTS * MOL_EVENT_WORDS),
    };
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
        const accepted = this.handleEvent(message);
        if (message.type === "control") {
          this.port.postMessage({
            type: "control-processed",
            requestId: integerInRange(message.requestId, 1, 0xffffffff)
              ? message.requestId
              : 0,
            accepted,
          });
        }
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
    if (message.type === "control") return this.handleControl(message);
    return false;
  }

  handleControl(message) {
    switch (message.control) {
      case "master-gain":
        return numberInRange(message.value, 0, 1) &&
          submitScalar(COMMAND.masterGain, message.value) === 0;
      case "sustain":
        return numberInRange(message.value, 0, 1) &&
          submitScalar(COMMAND.sustain, message.value) === 0;
      case "octave":
        return integerInRange(message.value, -2, 2) &&
          submitInteger(COMMAND.octave, message.value) === 0;
      case "transpose":
        return integerInRange(message.value, -24, 24) &&
          submitInteger(COMMAND.transpose, message.value) === 0;
      case "preset":
        return integerInRange(message.value, 0, 17) &&
          submitPreset(message.value, message.hardSwitch === true ? 1 : 0) === 0;
      case "parameter":
        return integerInRange(message.parameter, 1, 12) && Number.isFinite(message.value) &&
          submitParameter(message.parameter, message.value) === 0;
      case "scale":
        return integerInRange(message.scale, 0, 7) &&
          integerInRange(message.tonic, 0, 11) &&
          integerInRange(message.mapping, 0, 2) &&
          submitScale(message.scale, message.tonic, message.mapping) === 0;
      case "chord":
        return integerInRange(message.value, 0, 9) &&
          submitInteger(COMMAND.chord, message.value) === 0;
      case "arpeggiator":
        return integerInRange(message.mode, 0, 6) &&
          integerInRange(message.rate, 0, 5) &&
          numberInRange(message.gate, 0.05, 1) &&
          integerInRange(message.octaves, 1, 4) &&
          integerInRange(message.seed, 0, 0xffffffff) &&
          submitArpeggiator(
            message.mode,
            message.rate,
            message.gate,
            message.octaves,
            message.seed,
          ) === 0;
      case "tempo":
        return numberInRange(message.value, 30, 300) &&
          submitScalar(COMMAND.tempo, message.value) === 0;
      case "time-signature":
        return integerInRange(message.numerator, 1, 16) &&
          [2, 4, 8, 16].includes(message.denominator) &&
          submitTimeSignature(message.numerator, message.denominator) === 0;
      case "metronome":
        return typeof message.enabled === "boolean" && numberInRange(message.level, 0, 1) &&
          submitMetronome(message.enabled ? 1 : 0, message.level) === 0;
      case "portamento":
        return integerInRange(message.mode, 0, 2) &&
          numberInRange(message.timeMs, 0, 2000) &&
          submitPortamento(message.mode, message.timeMs) === 0;
      case "action": {
        const command = {
          "all-notes-off": COMMAND.allNotesOff,
          "all-sound-off": COMMAND.allSoundOff,
          "transport-start": COMMAND.transportStart,
          "transport-stop": COMMAND.transportStop,
          "record-start": COMMAND.recordStart,
          "record-stop": COMMAND.recordStop,
          "playback-start": COMMAND.playbackStart,
          "playback-stop": COMMAND.playbackStop,
          reset: COMMAND.reset,
        }[message.action];
        return command !== undefined && submitAction(command) === 0;
      }
      default:
        return false;
    }
  }

  postEngineEvents() {
    const count = pollEvents();
    if (count <= 0 || count > MOL_MAX_MESSAGE_EVENTS) return;
    const source = eventBuffer() >>> 2;
    const wordCount = count * MOL_EVENT_WORDS;
    for (let index = 0; index < wordCount; index += 1) {
      this.engineEventMessage.words[index] = molModule.HEAPU32[source + index];
    }
    this.engineEventMessage.count = count;
    this.port.postMessage(this.engineEventMessage);
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

    this.postEngineEvents();

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
