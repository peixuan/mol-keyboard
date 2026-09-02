import assert from "node:assert/strict";
import test from "node:test";

import { hasAndroidNativeBridge, NativeAudioEngine } from "../src/native-engine.ts";

interface NativeRequest {
  readonly version: number;
  readonly method: string;
  readonly params: Record<string, boolean | number | string>;
}

function installBridge(
  handler: (request: NativeRequest) => Record<string, unknown>,
): () => void {
  const previous = Object.getOwnPropertyDescriptor(globalThis, "window");
  const bridgeWindow = {
    MolKeyboardNative: {
      dispatch(text: string): string {
        return JSON.stringify(handler(JSON.parse(text) as NativeRequest));
      },
    },
    clearInterval,
    setInterval,
    setTimeout,
  } as unknown as Window;
  Object.defineProperty(globalThis, "window", {
    configurable: true,
    value: bridgeWindow,
    writable: true,
  });
  return () => {
    if (previous === undefined) delete (globalThis as { window?: Window }).window;
    else Object.defineProperty(globalThis, "window", previous);
  };
}

function wait(milliseconds: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

test("native bridge starts, reports its route, and transports complete commands", async () => {
  const requests: NativeRequest[] = [];
  let routeRevision = 1;
  let eventBatch = [13, 42, 1024, 60, 1];
  const restore = installBridge((request) => {
    requests.push(request);
    switch (request.method) {
      case "runtime.start":
        return { ok: true };
      case "runtime.status":
        return { active: true, ok: true, routeRevision, sampleRate: 48_000 };
      case "events.poll": {
        const events = eventBatch;
        eventBatch = [];
        return { events, ok: true };
      }
      case "command.submit":
        return { ok: true, result: 0 };
      default:
        return { error: "unexpected method", ok: false };
    }
  });

  try {
    assert.equal(hasAndroidNativeBridge(), true);
    const engine = new NativeAudioEngine();
    const batches: unknown[] = [];
    let deviceChanges = 0;
    engine.addEventListener("engineevents", (event) => {
      batches.push((event as CustomEvent).detail);
    });
    engine.addEventListener("devicechange", () => {
      deviceChanges += 1;
    });

    await engine.start();
    assert.equal(engine.state, "running");
    assert.equal(engine.sampleRate, 48_000);
    assert.equal(engine.commandTransport, "native-bridge");

    engine.noteOn(64, 0.75, 99);
    engine.noteOff(99);
    assert.equal(await engine.setScale(3, 5, 1), true);
    assert.equal(await engine.setArpeggiator(2, 4, 0.65, 3, 1234), true);
    assert.equal(await engine.action("transport-start"), true);
    assert.equal(await engine.action("unknown"), false);

    routeRevision = 2;
    await wait(80);
    assert.ok(deviceChanges >= 2);
    assert.deepEqual(batches[0], [
      { detail: 1, frame: 1024, gestureId: 42, note: 60, type: 13 },
    ]);

    const commands = requests.filter((request) => request.method === "command.submit");
    assert.deepEqual(commands.slice(0, 5).map((request) => request.params), [
      { f0: 0.75, f1: 0, gesture: 99, i0: 64, i1: 0, i2: 0, i3: 0, type: 1 },
      { f0: 0, f1: 0, gesture: 99, i0: 0, i1: 0, i2: 0, i3: 0, type: 2 },
      { f0: 0, f1: 0, gesture: 0, i0: 3, i1: 5, i2: 1, i3: 0, type: 13 },
      { f0: 0.65, f1: 0, gesture: 0, i0: 2, i1: 4, i2: 3, i3: 1234, type: 15 },
      { f0: 0, f1: 0, gesture: 0, i0: 0, i1: 0, i2: 0, i3: 0, type: 18 },
    ]);
    await engine.close();
  } finally {
    restore();
  }
});

test("native recording transfer is bounded and bridge failures are contained", async () => {
  let rejectCommands = false;
  const restore = installBridge((request) => {
    if (request.method === "runtime.start") return { ok: true };
    if (request.method === "runtime.status") {
      return { active: true, ok: true, routeRevision: 0, sampleRate: 44_100 };
    }
    if (request.method === "events.poll") return { events: [], ok: true };
    if (request.method === "recording.export") return { base64: "AQID", ok: true };
    if (request.method === "recording.load") {
      return { ok: request.params.base64 === "AQID", result: 0 };
    }
    if (request.method === "command.submit") {
      return rejectCommands ? { error: "queue full", ok: false } : { ok: true };
    }
    return { error: "unexpected method", ok: false };
  });

  try {
    const engine = new NativeAudioEngine();
    await engine.start();
    assert.deepEqual(await engine.exportRecording(), new Uint8Array([1, 2, 3]));
    assert.equal(await engine.loadRecording(new Uint8Array([1, 2, 3])), true);
    assert.equal(await engine.loadRecording(new Uint8Array()), false);
    assert.equal(await engine.loadRecording(new Uint8Array(2 * 1024 * 1024 + 1)), false);

    rejectCommands = true;
    engine.noteOn(60, 0.8, 7);
    assert.equal(engine.droppedCommandCount, 1);
    assert.equal(await engine.setMasterGain(0.5), false);
    await engine.close();
  } finally {
    restore();
  }
});
