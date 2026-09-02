export interface NoteEvent {
  readonly type: "note-on" | "note-off";
  readonly note?: number;
  readonly velocity?: number;
  readonly gestureId: number;
}

interface ReadyMessage {
  readonly type: "ready";
  readonly ready: boolean;
}

export interface EngineEvent {
  readonly type: number;
  readonly gestureId: number;
  readonly frame: number;
  readonly note: number;
  readonly detail: number;
}

interface ControlPayload {
  readonly control: string;
  readonly [key: string]: string | number | boolean;
}

const MAX_BATCH_SIZE = 64;
const READY_TIMEOUT_MS = 8_000;
const CONTROL_TIMEOUT_MS = 2_000;

export class MolAudioEngine extends EventTarget {
  private context: AudioContext | undefined;
  private node: AudioWorkletNode | undefined;
  private startPromise: Promise<void> | undefined;
  private readonly queue: NoteEvent[] = [];
  private flushScheduled = false;
  private nextRequestId = 1;
  private readonly pendingControls = new Map<
    number,
    { readonly resolve: (accepted: boolean) => void; readonly timer: number }
  >();
  private readonly pendingExports = new Map<
    number,
    { readonly resolve: (bytes: Uint8Array | undefined) => void; readonly timer: number }
  >();
  private readonly pendingLoads = new Map<
    number,
    { readonly resolve: (accepted: boolean) => void; readonly timer: number }
  >();

  get state(): AudioContextState | "idle" {
    return this.context?.state ?? "idle";
  }

  get sampleRate(): number | undefined {
    return this.context?.sampleRate;
  }

  start(): Promise<void> {
    this.startPromise ??= this.initialize();
    return this.startPromise;
  }

  noteOn(note: number, velocity: number, gestureId: number): void {
    this.enqueue({ type: "note-on", note, velocity, gestureId });
  }

  noteOff(gestureId: number): void {
    this.enqueue({ type: "note-off", gestureId });
  }

  allNotesOff(): void {
    this.queue.length = 0;
    this.node?.port.postMessage({ type: "all-notes-off" });
  }

  setMasterGain(value: number): Promise<boolean> {
    return this.sendControl({ control: "master-gain", value });
  }

  setSustain(value: number): Promise<boolean> {
    return this.sendControl({ control: "sustain", value });
  }

  setOctave(value: number): Promise<boolean> {
    return this.sendControl({ control: "octave", value });
  }

  setTranspose(value: number): Promise<boolean> {
    return this.sendControl({ control: "transpose", value });
  }

  setPreset(value: number, hardSwitch = false): Promise<boolean> {
    return this.sendControl({ control: "preset", value, hardSwitch });
  }

  setParameter(parameter: number, value: number): Promise<boolean> {
    return this.sendControl({ control: "parameter", parameter, value });
  }

  setScale(scale: number, tonic: number, mapping: number): Promise<boolean> {
    return this.sendControl({ control: "scale", scale, tonic, mapping });
  }

  setChord(value: number): Promise<boolean> {
    return this.sendControl({ control: "chord", value });
  }

  setArpeggiator(mode: number, rate: number, gate: number, octaves: number, seed: number): Promise<boolean> {
    return this.sendControl({ control: "arpeggiator", mode, rate, gate, octaves, seed });
  }

  setTempo(value: number): Promise<boolean> {
    return this.sendControl({ control: "tempo", value });
  }

  setTimeSignature(numerator: number, denominator: number): Promise<boolean> {
    return this.sendControl({ control: "time-signature", numerator, denominator });
  }

  setMetronome(enabled: boolean, level: number): Promise<boolean> {
    return this.sendControl({ control: "metronome", enabled, level });
  }

  setPortamento(mode: number, timeMs: number): Promise<boolean> {
    return this.sendControl({ control: "portamento", mode, timeMs });
  }

  action(action: string): Promise<boolean> {
    return this.sendControl({ control: "action", action });
  }

  async exportRecording(): Promise<Uint8Array | undefined> {
    await this.start();
    if (this.node === undefined) return undefined;
    const requestId = this.allocateRequestId();
    return new Promise((resolve) => {
      const timer = window.setTimeout(() => {
        this.pendingExports.delete(requestId);
        resolve(undefined);
      }, CONTROL_TIMEOUT_MS);
      this.pendingExports.set(requestId, { resolve, timer });
      this.node?.port.postMessage({ type: "recording-export", requestId });
    });
  }

  async loadRecording(bytes: Uint8Array): Promise<boolean> {
    await this.start();
    if (this.node === undefined || bytes.length === 0) return false;
    const requestId = this.allocateRequestId();
    const payload = bytes.slice();
    return new Promise((resolve) => {
      const timer = window.setTimeout(() => {
        this.pendingLoads.delete(requestId);
        resolve(false);
      }, CONTROL_TIMEOUT_MS);
      this.pendingLoads.set(requestId, { resolve, timer });
      this.node?.port.postMessage({ type: "recording-load", requestId, bytes: payload }, [
        payload.buffer,
      ]);
    });
  }

  async close(): Promise<void> {
    this.allNotesOff();
    this.node?.disconnect();
    this.node = undefined;
    if (this.context !== undefined && this.context.state !== "closed") {
      await this.context.close();
    }
    this.context = undefined;
    this.startPromise = undefined;
    for (const pending of this.pendingControls.values()) {
      window.clearTimeout(pending.timer);
      pending.resolve(false);
    }
    this.pendingControls.clear();
    for (const pending of this.pendingExports.values()) {
      window.clearTimeout(pending.timer);
      pending.resolve(undefined);
    }
    this.pendingExports.clear();
    for (const pending of this.pendingLoads.values()) {
      window.clearTimeout(pending.timer);
      pending.resolve(false);
    }
    this.pendingLoads.clear();
  }

  private async initialize(): Promise<void> {
    if (typeof AudioContext === "undefined" || typeof AudioWorkletNode === "undefined") {
      throw new Error("This browser does not support AudioWorklet.");
    }

    const context = new AudioContext({ latencyHint: "interactive" });
    this.context = context;
    try {
      const workletUrl = new URL("generated/mol_audio_worklet_core.js", document.baseURI);
      await context.audioWorklet.addModule(workletUrl.href);
      const node = new AudioWorkletNode(context, "mol-keyboard", {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2],
      });
      this.node = node;
      node.port.addEventListener("message", (event: MessageEvent<unknown>) => this.onMessage(event.data));
      node.connect(context.destination);
      await this.waitUntilReady(node.port);
      await context.resume();
      this.dispatchEvent(new Event("statechange"));
    } catch (error: unknown) {
      await context.close();
      this.context = undefined;
      this.node = undefined;
      this.startPromise = undefined;
      throw error;
    }
  }

  private async sendControl(payload: ControlPayload): Promise<boolean> {
    await this.start();
    if (this.node === undefined) return false;
    const requestId = this.allocateRequestId();
    return new Promise((resolve) => {
      const timer = window.setTimeout(() => {
        this.pendingControls.delete(requestId);
        resolve(false);
      }, CONTROL_TIMEOUT_MS);
      this.pendingControls.set(requestId, { resolve, timer });
      this.node?.port.postMessage({ type: "control", requestId, ...payload });
    });
  }

  private onMessage(data: unknown): void {
    if (data === null || typeof data !== "object") return;
    const message = data as {
      readonly type?: unknown;
      readonly requestId?: number;
      readonly accepted?: boolean;
      readonly count?: number;
      readonly words?: Uint32Array;
      readonly bytes?: Uint8Array;
    };
    if (message.type === "control-processed" && Number.isInteger(message.requestId)) {
      const pending = this.pendingControls.get(message.requestId ?? 0);
      if (pending === undefined) return;
      window.clearTimeout(pending.timer);
      this.pendingControls.delete(message.requestId ?? 0);
      pending.resolve(message.accepted === true);
      return;
    }
    if (message.type === "recording-exported" && Number.isInteger(message.requestId)) {
      const requestId = message.requestId ?? 0;
      const pending = this.pendingExports.get(requestId);
      if (pending === undefined) return;
      window.clearTimeout(pending.timer);
      this.pendingExports.delete(requestId);
      pending.resolve(message.accepted === true ? message.bytes : undefined);
      return;
    }
    if (message.type === "recording-loaded" && Number.isInteger(message.requestId)) {
      const requestId = message.requestId ?? 0;
      const pending = this.pendingLoads.get(requestId);
      if (pending === undefined) return;
      window.clearTimeout(pending.timer);
      this.pendingLoads.delete(requestId);
      pending.resolve(message.accepted === true);
      return;
    }
    if (
      message.type !== "engine-events" ||
      !Number.isInteger(message.count) ||
      message.count === undefined ||
      message.count < 0 ||
      message.count > MAX_BATCH_SIZE ||
      !(message.words instanceof Uint32Array)
    ) {
      return;
    }
    const events: EngineEvent[] = [];
    for (let index = 0; index < message.count; index += 1) {
      const offset = index * 4;
      events.push({
        type: message.words[offset] ?? 0,
        gestureId: message.words[offset + 1] ?? 0,
        frame: message.words[offset + 2] ?? 0,
        note: message.words[offset + 3] ?? 0,
        detail: message.words[offset + 3] ?? 0,
      });
    }
    this.dispatchEvent(new CustomEvent<readonly EngineEvent[]>("engineevents", { detail: events }));
  }

  private allocateRequestId(): number {
    const id = this.nextRequestId;
    this.nextRequestId = id === 0xffffffff ? 1 : id + 1;
    return id;
  }

  private waitUntilReady(port: MessagePort): Promise<void> {
    return new Promise((resolve, reject) => {
      const timer = window.setTimeout(() => {
        port.removeEventListener("message", onMessage);
        reject(new Error("The audio engine did not become ready in time."));
      }, READY_TIMEOUT_MS);
      const onMessage = (event: MessageEvent<unknown>): void => {
        const message = event.data as Partial<ReadyMessage> | null;
        if (message?.type !== "ready") return;
        window.clearTimeout(timer);
        port.removeEventListener("message", onMessage);
        if (message.ready === true) {
          resolve();
        } else {
          reject(new Error("The WebAssembly audio engine could not initialize."));
        }
      };
      port.addEventListener("message", onMessage);
      port.start();
    });
  }

  private enqueue(event: NoteEvent): void {
    if (this.node === undefined) return;
    this.queue.push(event);
    if (this.queue.length >= MAX_BATCH_SIZE) {
      this.flush();
      return;
    }
    if (!this.flushScheduled) {
      this.flushScheduled = true;
      queueMicrotask(() => this.flush());
    }
  }

  private flush(): void {
    this.flushScheduled = false;
    while (this.queue.length > 0) {
      const events = this.queue.splice(0, MAX_BATCH_SIZE);
      this.node?.port.postMessage({ type: "events", events });
    }
  }
}
