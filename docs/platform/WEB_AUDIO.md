# Web Audio Integration

MoL Keyboard's minimal Web target runs the portable C11 engine inside an
`AudioWorkletProcessor`. The generated `mol_audio_worklet_core.js` file is a
self-contained JavaScript and WebAssembly module and registers the processor
name `mol-keyboard`.

Serve the module over HTTP with a JavaScript MIME type and register it before
constructing the node:

```javascript
await audioContext.audioWorklet.addModule("./mol_audio_worklet_core.js");
const node = new AudioWorkletNode(audioContext, "mol-keyboard", {
  numberOfInputs: 0,
  numberOfOutputs: 1,
  outputChannelCount: [2],
});
node.connect(audioContext.destination);
```

The main thread controls notes through the node's message port. Gesture IDs
pair note-on and note-off messages deterministically:

```javascript
node.port.postMessage({
  type: "note-on",
  note: 60,
  velocity: 0.8,
  gestureId: 1,
});
node.port.postMessage({ type: "note-off", gestureId: 1 });
```

Live `AudioContext` playback must be resumed from a user gesture according to
browser autoplay rules. The repository's browser smoke page instead uses
`OfflineAudioContext`, so it can verify registration and deterministic rendered
audio without opening an output device.

The processor intentionally has no network fetching, asynchronous setup,
allocation, logging, blocking operation, or message posting in its render
method. The Wasm heap is fixed at link time, and engine memory remains bounded.
