import type { AudioBackend } from "./audio-backend";
import type { EngineEvent } from "./audio-engine";

type JsonPrimitive = boolean | number | string | null;
type JsonValue = JsonPrimitive | JsonValue[] | { readonly [key: string]: JsonValue };
type JsonObject = { readonly [key: string]: JsonValue };

const MAXIMUM_MESSAGE_BYTES = 65_536;
const REQUEST_TIMEOUT_MS = 2_000;

function isObject(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function isSafeInteger(value: unknown, minimum: number, maximum: number): value is number {
  return Number.isSafeInteger(value) && Number(value) >= minimum && Number(value) <= maximum;
}

function accepted(value: JsonValue): boolean {
  return isObject(value) && value.ok === true;
}

export function buildServiceWebSocketUrl(endpoint: string, token: string): string {
  if (!/^[0-9a-f]{32,128}$/iu.test(token)) throw new Error("Enter the service session token");
  let url: URL;
  try {
    url = new URL(endpoint);
  } catch {
    throw new Error("The service endpoint is not a valid URL");
  }
  if (
    url.protocol !== "ws:" ||
    (url.hostname !== "127.0.0.1" && url.hostname !== "localhost") ||
    url.pathname !== "/control" ||
    url.username !== "" ||
    url.password !== "" ||
    url.search !== "" ||
    url.hash !== ""
  ) {
    throw new Error("Use a loopback ws://127.0.0.1:PORT/control endpoint");
  }
  url.searchParams.set("token", token);
  return url.href;
}

export class ServiceAudioEngine extends EventTarget implements AudioBackend {
  private socket: WebSocket | undefined;
  private currentState: AudioContextState | "idle" = "idle";
  private currentSampleRate: number | undefined;
  private nextRequestId = 1;
  private droppedCommands = 0;
  private lastSavedRecording: string | undefined;
  private readonly pending = new Map<
    number,
    {
      readonly resolve: (value: JsonValue) => void;
      readonly reject: (error: Error) => void;
      readonly timer: number;
    }
  >();

  get state(): AudioContextState | "idle" {
    return this.currentState;
  }

  get sampleRate(): number | undefined {
    return this.currentSampleRate;
  }

  get commandTransport(): "websocket-jsonrpc" {
    return "websocket-jsonrpc";
  }

  get droppedCommandCount(): number {
    return this.droppedCommands;
  }

  get connected(): boolean {
    return this.socket?.readyState === WebSocket.OPEN;
  }

  get lastRemoteRecording(): string | undefined {
    return this.lastSavedRecording;
  }

  async connect(endpoint: string, token: string): Promise<void> {
    await this.close();
    const url = buildServiceWebSocketUrl(endpoint, token);
    const socket = new WebSocket(url);
    this.socket = socket;
    socket.addEventListener("message", (event) => this.onMessage(event));
    socket.addEventListener("close", () => this.onClose());
    socket.addEventListener("error", () => {
      if (socket.readyState !== WebSocket.OPEN) this.onClose();
    });
    await new Promise<void>((resolve, reject) => {
      const timer = window.setTimeout(() => {
        socket.close();
        reject(new Error("Timed out connecting to the desktop service"));
      }, REQUEST_TIMEOUT_MS);
      socket.addEventListener(
        "open",
        () => {
          window.clearTimeout(timer);
          resolve();
        },
        { once: true },
      );
      socket.addEventListener(
        "close",
        () => {
          window.clearTimeout(timer);
          reject(new Error("The desktop service rejected the connection"));
        },
        { once: true },
      );
    });
    const [info, latency] = await Promise.all([
      this.request("system.getInfo", {}),
      this.request("audio.getLatency", {}),
    ]);
    if (!isObject(info) || info.name !== "MoL Keyboard Service" || !isObject(latency)) {
      await this.close();
      throw new Error("The endpoint is not a compatible MoL Keyboard service");
    }
    const sampleRate = latency.sample_rate;
    if (!isSafeInteger(sampleRate, 8_000, 384_000)) {
      await this.close();
      throw new Error("The service returned an invalid audio sample rate");
    }
    this.currentSampleRate = sampleRate;
    this.currentState = "running";
    this.dispatchEvent(new Event("statechange"));
  }

  async start(): Promise<void> {
    if (!this.connected) throw new Error("Connect to the desktop service first");
  }

  async close(): Promise<void> {
    const socket = this.socket;
    if (socket !== undefined && socket.readyState === WebSocket.OPEN) {
      this.notify("engine.allNotesOff", {});
      socket.close(1000, "controller closed");
    } else if (socket?.readyState === WebSocket.CONNECTING) {
      socket.close();
    }
    this.socket = undefined;
    this.onClose();
  }

  noteOn(note: number, velocity: number, gestureId: number): void {
    this.notify("performance.noteOn", { note, velocity, gesture: gestureId });
  }

  noteOff(gestureId: number): void {
    this.notify("performance.noteOff", { gesture: gestureId });
  }

  allNotesOff(): void {
    if (this.connected) this.notify("engine.allNotesOff", {});
  }

  setMasterGain(value: number): Promise<boolean> {
    return this.control({ control: "master-gain", value });
  }

  setSustain(value: number): Promise<boolean> {
    return this.control({ control: "sustain", value });
  }

  setOctave(value: number): Promise<boolean> {
    return this.control({ control: "octave", value });
  }

  setTranspose(value: number): Promise<boolean> {
    return this.control({ control: "transpose", value });
  }

  async setPreset(value: number, hardSwitch = false): Promise<boolean> {
    return accepted(await this.request("preset.select", { preset: value, hard: hardSwitch }));
  }

  async setParameter(parameter: number, value: number): Promise<boolean> {
    return accepted(await this.request("preset.setParameter", { parameter, value }));
  }

  setScale(scale: number, tonic: number, mapping: number): Promise<boolean> {
    return this.control({ control: "scale", type: scale, tonic, mapping });
  }

  setChord(value: number): Promise<boolean> {
    return this.control({ control: "chord", value });
  }

  setArpeggiator(
    mode: number,
    rate: number,
    gate: number,
    octaves: number,
    seed: number,
  ): Promise<boolean> {
    return this.control({
      control: "arpeggiator",
      mode,
      rate,
      gate,
      octaves,
      random_seed: seed,
    });
  }

  async setTempo(value: number): Promise<boolean> {
    return accepted(await this.request("transport.setTempo", { bpm: value }));
  }

  async setTimeSignature(numerator: number, denominator: number): Promise<boolean> {
    return accepted(
      await this.request("transport.setTimeSignature", { numerator, denominator }),
    );
  }

  setMetronome(enabled: boolean, level: number): Promise<boolean> {
    return this.control({ control: "metronome", enabled, level });
  }

  setPortamento(mode: number, timeMs: number): Promise<boolean> {
    return this.control({ control: "portamento", mode, time_ms: timeMs });
  }

  async action(action: string): Promise<boolean> {
    const methods: Record<string, string> = {
      "record-start": "recording.start",
      "playback-start": "playback.start",
      "playback-stop": "playback.stop",
    };
    if (action === "record-stop") {
      const timestamp = new Date().toISOString().replaceAll(":", "-").replace(".", "-");
      this.lastSavedRecording = `web-${timestamp}.molseq`;
      return accepted(
        await this.request("recording.stop", { name: this.lastSavedRecording }),
      );
    }
    const method = methods[action];
    return method === undefined ? false : accepted(await this.request(method, {}));
  }

  exportRecording(): Promise<Uint8Array | undefined> {
    return Promise.resolve(undefined);
  }

  loadRecording(_bytes: Uint8Array): Promise<boolean> {
    return Promise.resolve(false);
  }

  async listRemoteRecordings(): Promise<readonly string[]> {
    const result = await this.request("recording.list", {});
    if (!Array.isArray(result) || result.length > 1_024 || result.some((item) => typeof item !== "string"))
      throw new Error("The service returned an invalid recording list");
    return result as string[];
  }

  async loadRemoteRecording(name: string): Promise<boolean> {
    return accepted(await this.request("recording.load", { name }));
  }

  private async control(params: JsonObject): Promise<boolean> {
    return accepted(await this.request("performance.control", params));
  }

  private notify(method: string, params: JsonObject): void {
    void this.request(method, params).catch(() => {
      this.droppedCommands += 1;
    });
  }

  private request(method: string, params: JsonObject): Promise<JsonValue> {
    const socket = this.socket;
    if (socket === undefined || socket.readyState !== WebSocket.OPEN)
      return Promise.reject(new Error("The desktop service is not connected"));
    const id = this.allocateRequestId();
    const message = JSON.stringify({ jsonrpc: "2.0", method, params, id });
    if (message.length > MAXIMUM_MESSAGE_BYTES)
      return Promise.reject(new Error("The service request exceeds the 64 KiB limit"));
    return new Promise((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`The service did not answer ${method}`));
      }, REQUEST_TIMEOUT_MS);
      this.pending.set(id, { resolve, reject, timer });
      socket.send(message);
    });
  }

  private onMessage(event: MessageEvent<unknown>): void {
    if (typeof event.data !== "string" || event.data.length > MAXIMUM_MESSAGE_BYTES) {
      this.protocolFailure("The service sent an invalid message");
      return;
    }
    let message: unknown;
    try {
      message = JSON.parse(event.data) as unknown;
    } catch {
      this.protocolFailure("The service sent malformed JSON");
      return;
    }
    if (!isObject(message) || message.jsonrpc !== "2.0") {
      this.protocolFailure("The service sent an invalid JSON-RPC envelope");
      return;
    }
    if (message.method === "engine.events") {
      this.onEngineEvents(message.params);
      return;
    }
    if (!isSafeInteger(message.id, 1, Number.MAX_SAFE_INTEGER)) {
      this.protocolFailure("The service response has an invalid request id");
      return;
    }
    const pending = this.pending.get(message.id);
    if (pending === undefined) return;
    window.clearTimeout(pending.timer);
    this.pending.delete(message.id);
    if (message.error !== undefined) {
      const rpcError = isObject(message.error) ? message.error : {};
      const text = typeof rpcError.message === "string" ? rpcError.message : "RPC request failed";
      pending.reject(new Error(text.slice(0, 256)));
    } else if (message.result !== undefined) {
      pending.resolve(message.result as JsonValue);
    } else {
      pending.reject(new Error("The service response has neither a result nor an error"));
    }
  }

  private onEngineEvents(params: unknown): void {
    if (!isObject(params) || !Array.isArray(params.events) || params.events.length > 64) {
      this.protocolFailure("The service event batch is invalid");
      return;
    }
    const events: EngineEvent[] = [];
    for (const value of params.events) {
      if (
        !isObject(value) ||
        !isSafeInteger(value.type, 0, 255) ||
        !isSafeInteger(value.gesture, 0, Number.MAX_SAFE_INTEGER) ||
        !isSafeInteger(value.frame, 0, Number.MAX_SAFE_INTEGER) ||
        !isSafeInteger(value.note, 0, 127) ||
        !isSafeInteger(value.detail, 0, 255)
      ) {
        this.protocolFailure("The service sent an invalid engine event");
        return;
      }
      events.push({
        type: value.type,
        gestureId: value.gesture,
        frame: value.frame,
        note: value.note,
        detail: value.detail,
      });
    }
    this.dispatchEvent(new CustomEvent<readonly EngineEvent[]>("engineevents", { detail: events }));
  }

  private protocolFailure(message: string): void {
    this.socket?.close(1002, "protocol error");
    this.failPending(new Error(message));
  }

  private onClose(): void {
    this.currentState = "idle";
    this.currentSampleRate = undefined;
    this.failPending(new Error("The desktop service connection closed"));
    this.dispatchEvent(new Event("statechange"));
  }

  private failPending(error: Error): void {
    for (const pending of this.pending.values()) {
      window.clearTimeout(pending.timer);
      pending.reject(error);
    }
    this.pending.clear();
  }

  private allocateRequestId(): number {
    const id = this.nextRequestId;
    this.nextRequestId = id >= Number.MAX_SAFE_INTEGER ? 1 : id + 1;
    return id;
  }
}
