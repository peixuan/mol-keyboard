// SPDX-License-Identifier: Apache-2.0
import { pathToFileURL } from "node:url";

const moduleUrl = pathToFileURL(process.argv[2]).href;
const { default: createModule } = await import(moduleUrl);
const module = await createModule();
const initialize = module.cwrap("mol_wasm_initialize", "number", ["number", "number", "number"]);
const noteOn = module.cwrap("mol_wasm_note_on", "number", ["number", "number", "number"]);
const submitInteger = module.cwrap("mol_wasm_submit_integer", "number", ["number", "number"]);
const pollEvents = module.cwrap("mol_wasm_poll_events", "number", []);
const eventBuffer = module.cwrap("mol_wasm_event_buffer", "number", []);
const render = module.cwrap("mol_wasm_render", "number", ["number", "number"]);

if (
  initialize(48000, 1, 8) !== 0 ||
  submitInteger(11, 0) !== 0 ||
  submitInteger(99, 0) === 0 ||
  noteOn(60, 1.0, 1) !== 0
) {
  throw new Error("Could not initialize the Wasm C4 conformance render");
}

let previous = 0.0;
let crossings = 0;
let peak = 0.0;
for (let block = 0; block < 375; block += 1) {
  const pointer = render(128, 1);
  if (pointer === 0) {
    throw new Error("Wasm render returned a null buffer");
  }
  const samples = module.HEAPF32.subarray(pointer >>> 2, (pointer >>> 2) + 128);
  for (let index = 0; index < samples.length; index += 1) {
    const frame = block * 128 + index;
    const sample = samples[index];
    if (!Number.isFinite(sample)) {
      throw new Error(`Non-finite Wasm sample at frame ${frame}`);
    }
    peak = Math.max(peak, Math.abs(sample));
    if (frame >= 4800 && frame < 43200 && previous <= 0.0 && sample > 0.0) {
      crossings += 1;
    }
    previous = sample;
  }
}

const eventCount = pollEvents();
const eventWords = module.HEAPU32.subarray(
  eventBuffer() >>> 2,
  (eventBuffer() >>> 2) + eventCount * 4,
);
if (
  eventCount < 1 ||
  !Array.from(eventWords).some(
    (word, index, words) => index % 4 === 0 && word === 1 && words[index + 3] === 60,
  )
) {
  throw new Error("Wasm event bridge did not expose the note-started event");
}

const frequency = crossings / 0.8;
if (peak <= 0.01 || peak > 1.0 || Math.abs(frequency - 261.6256) >= 1.0) {
  throw new Error(`Wasm C4 analysis failed: frequency=${frequency}, peak=${peak}`);
}
process.stdout.write(`Wasm C4 frequency=${frequency.toFixed(4)}Hz peak=${peak.toFixed(8)}\n`);
