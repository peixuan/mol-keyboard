# Web Audio Integration

MoL Keyboard's production Web target runs the portable C11 engine inside an
`AudioWorkletProcessor`. The generated `mol_audio_worklet_core.js` module
registers the processor name `mol-keyboard`; its paired
`mol_audio_worklet_core.wasm` contains the C engine.

Serve both files with their standard JavaScript and WebAssembly MIME types. Read
the Wasm on the main thread, register the worklet, and pass the bounded binary
to the node. This keeps worklet registration portable to browsers whose
`AudioWorkletGlobalScope` does not provide the URL or fetch APIs:

```javascript
const response = await fetch("./mol_audio_worklet_core.wasm");
if (!response.ok) throw new Error(`Wasm fetch failed: HTTP ${response.status}`);
const wasmBinary = await response.arrayBuffer();
await audioContext.audioWorklet.addModule("./mol_audio_worklet_core.js");
const node = new AudioWorkletNode(audioContext, "mol-keyboard", {
  numberOfInputs: 0,
  numberOfOutputs: 1,
  outputChannelCount: [2],
  processorOptions: { wasmBinary },
});
node.connect(audioContext.destination);
```

The baseline main-thread transport sends bounded batches through the node's
message port. Gesture IDs pair note-on and note-off messages deterministically.
When the server supplies COOP and COEP headers, the product instead offers a
preallocated SharedArrayBuffer SPSC ring; this is an optimization, not a
deployment requirement.

```javascript
node.port.postMessage({
  type: "note-on",
  note: 60,
  velocity: 0.8,
  gestureId: 1,
});
node.port.postMessage({ type: "note-off", gestureId: 1 });
```

Live `AudioContext` playback starts or resumes only from a user gesture according
to browser autoplay rules. The product suspends and releases every gesture on a
hidden page, page hide, or blur and handles audio-state and output-device
changes. The automated browser suite starts the real worklet and observes
engine events rather than substituting an offline renderer.

The processor's render method intentionally has no network fetching,
asynchronous setup, allocation, logging, blocking operation, or message
posting. Initialization is completed before the processor reports `ready`; the
Wasm heap is fixed at link time, and engine memory remains bounded.

Build and test the complete PWA with the pinned package lock:

```powershell
cd apps/web
npm ci
npm run test
npm run test:browser
```

`npm run preview` enables cross-origin isolation for the SharedArrayBuffer path.
Any ordinary static HTTPS server remains supported through MessagePort. See
`docs/web/M6_WEB_EVIDENCE.md` for the exact verified browser boundary.
