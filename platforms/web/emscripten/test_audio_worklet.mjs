// SPDX-License-Identifier: Apache-2.0
import { pathToFileURL } from "node:url";

let processorConstructor;

class MockMessagePort {
  constructor() {
    this.onmessage = undefined;
    this.messages = [];
  }

  postMessage(message) {
    this.messages.push(message);
  }
}

globalThis.AudioWorkletProcessor = class AudioWorkletProcessor {
  constructor() {
    this.port = new MockMessagePort();
  }
};
globalThis.sampleRate = 48000;
globalThis.window = {};
globalThis.registerProcessor = (name, constructor) => {
  if (name !== "mol-keyboard") {
    throw new Error(`Unexpected AudioWorklet processor name: ${name}`);
  }
  processorConstructor = constructor;
};

await import(pathToFileURL(process.argv[2]).href);
if (processorConstructor === undefined) {
  throw new Error("AudioWorklet processor was not registered");
}

const processor = new processorConstructor();
processor.port.onmessage({
  data: { type: "control", requestId: 6, control: "action", action: "record-start" },
});
processor.port.onmessage({
  data: {
    type: "events",
    events: [
      { type: "note-on", note: 60, velocity: 1.0, gestureId: 1 },
      { type: "note-on", note: 200, velocity: 1.0, gestureId: 2 },
    ],
  },
});
const batchResult = processor.port.messages.at(-1);
if (batchResult?.accepted !== 1 || batchResult?.rejected !== 1) {
  throw new Error(`AudioWorklet batch validation failed: ${JSON.stringify(batchResult)}`);
}

processor.port.onmessage({
  data: { type: "control", requestId: 7, control: "preset", value: 0 },
});
const acceptedControl = processor.port.messages.at(-1);
processor.port.onmessage({
  data: { type: "control", requestId: 8, control: "preset", value: 99 },
});
const rejectedControl = processor.port.messages.at(-1);
if (
  acceptedControl?.requestId !== 7 ||
  acceptedControl?.accepted !== true ||
  rejectedControl?.requestId !== 8 ||
  rejectedControl?.accepted !== false
) {
  throw new Error("AudioWorklet control validation failed");
}

const left = new Float32Array(128);
const right = new Float32Array(128);
let previous = 0.0;
let crossings = 0;
let peak = 0.0;
let stereoDifference = 0.0;
for (let block = 0; block < 375; block += 1) {
  if (!processor.process([], [[left, right]], {})) {
    throw new Error("AudioWorklet processor unexpectedly stopped");
  }
  for (let index = 0; index < left.length; index += 1) {
    const frame = block * left.length + index;
    const sample = left[index];
    if (!Number.isFinite(sample) || !Number.isFinite(right[index])) {
      throw new Error(`Invalid stereo AudioWorklet sample at frame ${frame}`);
    }
    peak = Math.max(peak, Math.abs(sample));
    stereoDifference += Math.abs(sample - right[index]);
    if (frame >= 4800 && frame < 43200 && previous <= 0.0 && sample > 0.0) {
      crossings += 1;
    }
    previous = sample;
  }
}

const engineEventBatch = processor.port.messages.find((message) => message.type === "engine-events");
if (
  engineEventBatch?.count < 1 ||
  !Array.from(engineEventBatch.words.slice(0, engineEventBatch.count * 4)).some(
    (word, index, words) => index % 4 === 0 && word === 1 && words[index + 3] === 60,
  )
) {
  throw new Error("AudioWorklet did not report the core note-started event");
}

const frequency = crossings / 0.8;
if (
  peak <= 0.01 ||
  peak > 1.0 ||
  stereoDifference <= 0.000001 ||
  Math.abs(frequency - 261.6256) >= 1.0
) {
  throw new Error(`AudioWorklet C4 analysis failed: frequency=${frequency}, peak=${peak}`);
}

processor.port.onmessage({ data: { type: "all-notes-off" } });
for (let block = 0; block < 1200; block += 1) {
  processor.process([], [[left, right]], {});
}
peak = 0.0;
for (const sample of left) {
  peak = Math.max(peak, Math.abs(sample));
}
if (peak > 0.000001) {
  throw new Error(`AudioWorklet release did not reach silence: peak=${peak}`);
}

processor.port.onmessage({
  data: { type: "control", requestId: 9, control: "action", action: "record-stop" },
});
processor.process([], [[left, right]], {});
processor.port.onmessage({ data: { type: "recording-export", requestId: 10 } });
const exported = processor.port.messages.at(-1);
const magic = exported?.bytes instanceof Uint8Array
  ? new TextDecoder().decode(exported.bytes.subarray(0, 4))
  : "";
if (exported?.accepted !== true || exported.bytes.length < 120 || magic !== "MOLS") {
  throw new Error(`AudioWorklet recording export failed: ${JSON.stringify(exported)}`);
}
processor.port.onmessage({
  data: { type: "recording-load", requestId: 11, bytes: exported.bytes },
});
const loaded = processor.port.messages.at(-1);
if (loaded?.requestId !== 11 || loaded?.accepted !== true) {
  throw new Error("AudioWorklet recording reload failed");
}

const sharedCapacity = 4;
const sharedBuffer = new SharedArrayBuffer((4 + sharedCapacity * 4) * 4);
const sharedWords = new Int32Array(sharedBuffer);
const sharedFloats = new Float32Array(sharedBuffer);
const sharedProcessor = new processorConstructor({
  processorOptions: { commandBuffer: sharedBuffer, commandCapacity: sharedCapacity },
});
const sharedOffset = 4;
sharedWords[sharedOffset] = 1;
sharedWords[sharedOffset + 1] = 64;
sharedFloats[sharedOffset + 2] = 0.8;
sharedWords[sharedOffset + 3] = 44;
Atomics.store(sharedWords, 0, 1);
sharedProcessor.process([], [[left, right]], {});
const sharedEvents = sharedProcessor.port.messages.find((message) => message.type === "engine-events");
if (
  sharedProcessor.port.messages[0]?.fastPath !== true ||
  Atomics.load(sharedWords, 1) !== 1 ||
  sharedEvents?.words[3] !== 64
) {
  throw new Error("AudioWorklet SharedArrayBuffer fast path failed");
}

process.stdout.write(
  `AudioWorklet C4 frequency=${frequency.toFixed(4)}Hz release=ok recording=${exported.bytes.length}B shared=ok\n`,
);
