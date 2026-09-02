// SPDX-License-Identifier: Apache-2.0
import { pathToFileURL } from "node:url";

let processorConstructor;

class MockMessagePort {
  constructor() {
    this.onmessage = undefined;
  }

  postMessage(_message) {}
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
  data: { type: "note-on", note: 60, velocity: 1.0, gestureId: 1 },
});

const left = new Float32Array(128);
const right = new Float32Array(128);
let previous = 0.0;
let crossings = 0;
let peak = 0.0;
for (let block = 0; block < 375; block += 1) {
  if (!processor.process([], [[left, right]], {})) {
    throw new Error("AudioWorklet processor unexpectedly stopped");
  }
  for (let index = 0; index < left.length; index += 1) {
    const frame = block * left.length + index;
    const sample = left[index];
    if (!Number.isFinite(sample) || sample !== right[index]) {
      throw new Error(`Invalid stereo AudioWorklet sample at frame ${frame}`);
    }
    peak = Math.max(peak, Math.abs(sample));
    if (frame >= 4800 && frame < 43200 && previous <= 0.0 && sample > 0.0) {
      crossings += 1;
    }
    previous = sample;
  }
}

const frequency = crossings / 0.8;
if (peak <= 0.1 || peak > 1.0 || Math.abs(frequency - 261.6256) >= 1.0) {
  throw new Error(`AudioWorklet C4 analysis failed: frequency=${frequency}, peak=${peak}`);
}

processor.port.onmessage({ data: { type: "note-off", gestureId: 1 } });
for (let block = 0; block < 400; block += 1) {
  processor.process([], [[left, right]], {});
}
peak = 0.0;
for (const sample of left) {
  peak = Math.max(peak, Math.abs(sample));
}
if (peak > 0.000001) {
  throw new Error(`AudioWorklet release did not reach silence: peak=${peak}`);
}

process.stdout.write(`AudioWorklet C4 frequency=${frequency.toFixed(4)}Hz release=ok\n`);
