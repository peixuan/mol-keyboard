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

const MAX_BATCH_SIZE = 64;
const READY_TIMEOUT_MS = 8_000;

export class MolAudioEngine extends EventTarget {
  private context: AudioContext | undefined;
  private node: AudioWorkletNode | undefined;
  private startPromise: Promise<void> | undefined;
  private readonly queue: NoteEvent[] = [];
  private flushScheduled = false;

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

  async close(): Promise<void> {
    this.allNotesOff();
    this.node?.disconnect();
    this.node = undefined;
    if (this.context !== undefined && this.context.state !== "closed") {
      await this.context.close();
    }
    this.context = undefined;
    this.startPromise = undefined;
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
