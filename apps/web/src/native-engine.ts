import type { AudioBackend } from "./audio-backend";
import type { EngineEvent } from "./audio-engine";

interface AndroidBridge {
  dispatch(request: string): string;
}

interface NativeWindow extends Window {
  readonly MolKeyboardNative?: AndroidBridge;
}

type NativeParams = Readonly<Record<string, boolean | number | string>>;
type NativeResponse = Readonly<Record<string, unknown>>;

const MAXIMUM_RECORDING_BYTES = 2 * 1024 * 1024;
const MAXIMUM_RESPONSE_CHARS = 2_800_000;
const EVENT_FIELD_COUNT = 5;
const MAXIMUM_EVENT_FIELDS = 64 * EVENT_FIELD_COUNT;
const START_TIMEOUT_MS = 3_000;
const POLL_INTERVAL_MS = 32;

const COMMAND = {
  noteOn: 1,
  noteOff: 2,
  sustain: 5,
  allNotesOff: 6,
  masterGain: 8,
  preset: 9,
  parameter: 10,
  octave: 11,
  transpose: 12,
  scale: 13,
  chord: 14,
  arpeggiator: 15,
  tempo: 16,
  timeSignature: 17,
  transportStart: 18,
  transportStop: 19,
  recordStart: 21,
  recordStop: 22,
  playbackStart: 23,
  playbackStop: 24,
  metronome: 27,
  portamento: 28,
} as const;

function isObject(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function finiteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value);
}

function binaryToBase64(bytes: Uint8Array): string {
  let binary = "";
  for (let offset = 0; offset < bytes.length; offset += 0x8000) {
    binary += String.fromCharCode(...bytes.subarray(offset, Math.min(bytes.length, offset + 0x8000)));
  }
  return btoa(binary);
}

function base64ToBinary(encoded: string): Uint8Array {
  const binary = atob(encoded);
  if (binary.length === 0 || binary.length > MAXIMUM_RECORDING_BYTES) {
    throw new Error("The native recording size is invalid");
  }
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; ++index) bytes[index] = binary.charCodeAt(index);
  return bytes;
}

export function hasAndroidNativeBridge(): boolean {
  return typeof window !== "undefined" &&
    typeof (window as NativeWindow).MolKeyboardNative?.dispatch === "function";
}

export class NativeAudioEngine extends EventTarget implements AudioBackend {
  private currentState: AudioContextState | "idle" = "idle";
  private currentSampleRate: number | undefined;
  private pollTimer: number | undefined;
  private droppedCommands = 0;
  private routeRevision = 0;

  get state(): AudioContextState | "idle" {
    return this.currentState;
  }

  get sampleRate(): number | undefined {
    return this.currentSampleRate;
  }

  get commandTransport(): "native-bridge" {
    return "native-bridge";
  }

  get droppedCommandCount(): number {
    return this.droppedCommands;
  }

  async start(): Promise<void> {
    this.request("runtime.start", {});
    const deadline = performance.now() + START_TIMEOUT_MS;
    while (performance.now() < deadline) {
      const status = this.tryRequest("runtime.status", {});
      if (status?.active === true) {
        this.applyStatus(status);
        this.startPolling();
        return;
      }
      await new Promise((resolve) => window.setTimeout(resolve, 25));
    }
    throw new Error("The native Oboe runtime did not start");
  }

  close(): Promise<void> {
    this.allNotesOff();
    if (this.pollTimer !== undefined) window.clearInterval(this.pollTimer);
    this.pollTimer = undefined;
    return Promise.resolve();
  }

  noteOn(note: number, velocity: number, gestureId: number): void {
    this.notify(COMMAND.noteOn, gestureId, note, 0, 0, 0, velocity);
  }

  noteOff(gestureId: number): void {
    this.notify(COMMAND.noteOff, gestureId);
  }

  allNotesOff(): void {
    if (this.currentState === "running") this.notify(COMMAND.allNotesOff);
  }

  setMasterGain(value: number): Promise<boolean> {
    return this.control(COMMAND.masterGain, 0, 0, 0, 0, value);
  }

  setSustain(value: number): Promise<boolean> {
    return this.control(COMMAND.sustain, 0, 0, 0, 0, value);
  }

  setOctave(value: number): Promise<boolean> {
    return this.control(COMMAND.octave, value);
  }

  setTranspose(value: number): Promise<boolean> {
    return this.control(COMMAND.transpose, value);
  }

  setPreset(value: number, hardSwitch = false): Promise<boolean> {
    return this.control(COMMAND.preset, value, hardSwitch ? 1 : 0);
  }

  setParameter(parameter: number, value: number): Promise<boolean> {
    return this.control(COMMAND.parameter, parameter, 0, 0, 0, value);
  }

  setScale(scale: number, tonic: number, mapping: number): Promise<boolean> {
    return this.control(COMMAND.scale, scale, tonic, mapping);
  }

  setChord(value: number): Promise<boolean> {
    return this.control(COMMAND.chord, value);
  }

  setArpeggiator(
    mode: number,
    rate: number,
    gate: number,
    octaves: number,
    seed: number,
  ): Promise<boolean> {
    return this.control(COMMAND.arpeggiator, mode, rate, octaves, seed, gate);
  }

  setTempo(value: number): Promise<boolean> {
    return this.control(COMMAND.tempo, 0, 0, 0, 0, value);
  }

  setTimeSignature(numerator: number, denominator: number): Promise<boolean> {
    return this.control(COMMAND.timeSignature, numerator, denominator);
  }

  setMetronome(enabled: boolean, level: number): Promise<boolean> {
    return this.control(COMMAND.metronome, enabled ? 1 : 0, 0, 0, 0, level);
  }

  setPortamento(mode: number, timeMs: number): Promise<boolean> {
    return this.control(COMMAND.portamento, mode, 0, 0, 0, timeMs);
  }

  action(action: string): Promise<boolean> {
    const commands: Readonly<Record<string, number>> = {
      "record-start": COMMAND.recordStart,
      "record-stop": COMMAND.recordStop,
      "playback-start": COMMAND.playbackStart,
      "playback-stop": COMMAND.playbackStop,
      "transport-start": COMMAND.transportStart,
      "transport-stop": COMMAND.transportStop,
    };
    const command = commands[action];
    return command === undefined ? Promise.resolve(false) : this.control(command);
  }

  exportRecording(): Promise<Uint8Array | undefined> {
    try {
      const response = this.request("recording.export", {});
      return Promise.resolve(
        typeof response.base64 === "string" ? base64ToBinary(response.base64) : undefined,
      );
    } catch {
      return Promise.resolve(undefined);
    }
  }

  loadRecording(bytes: Uint8Array): Promise<boolean> {
    if (bytes.length === 0 || bytes.length > MAXIMUM_RECORDING_BYTES) return Promise.resolve(false);
    try {
      return Promise.resolve(this.request("recording.load", { base64: binaryToBase64(bytes) }).ok === true);
    } catch {
      return Promise.resolve(false);
    }
  }

  private control(
    type: number,
    integer0 = 0,
    integer1 = 0,
    integer2 = 0,
    integer3 = 0,
    scalar0 = 0,
    scalar1 = 0,
  ): Promise<boolean> {
    try {
      return Promise.resolve(
        this.submit(type, 0, integer0, integer1, integer2, integer3, scalar0, scalar1),
      );
    } catch {
      return Promise.resolve(false);
    }
  }

  private notify(
    type: number,
    gesture = 0,
    integer0 = 0,
    integer1 = 0,
    integer2 = 0,
    integer3 = 0,
    scalar0 = 0,
    scalar1 = 0,
  ): void {
    try {
      if (!this.submit(type, gesture, integer0, integer1, integer2, integer3, scalar0, scalar1)) {
        this.droppedCommands += 1;
      }
    } catch {
      this.droppedCommands += 1;
    }
  }

  private submit(
    type: number,
    gesture: number,
    integer0: number,
    integer1: number,
    integer2: number,
    integer3: number,
    scalar0: number,
    scalar1: number,
  ): boolean {
    const response = this.request("command.submit", {
      type,
      gesture,
      i0: integer0,
      i1: integer1,
      i2: integer2,
      i3: integer3,
      f0: scalar0,
      f1: scalar1,
    });
    return response.ok === true;
  }

  private startPolling(): void {
    if (this.pollTimer !== undefined) return;
    this.pollTimer = window.setInterval(() => this.poll(), POLL_INTERVAL_MS);
  }

  private poll(): void {
    const status = this.tryRequest("runtime.status", {});
    if (status === undefined) return;
    this.applyStatus(status);
    const response = this.tryRequest("events.poll", {});
    if (response === undefined || !Array.isArray(response.events)) return;
    const fields = response.events;
    if (
      fields.length > MAXIMUM_EVENT_FIELDS ||
      fields.length % EVENT_FIELD_COUNT !== 0 ||
      fields.some((field) => !Number.isSafeInteger(field))
    ) {
      this.droppedCommands += 1;
      return;
    }
    const events: EngineEvent[] = [];
    for (let offset = 0; offset < fields.length; offset += EVENT_FIELD_COUNT) {
      events.push({
        type: Number(fields[offset]),
        gestureId: Number(fields[offset + 1]),
        frame: Number(fields[offset + 2]),
        note: Number(fields[offset + 3]),
        detail: Number(fields[offset + 4]),
      });
    }
    if (events.length > 0) {
      this.dispatchEvent(new CustomEvent<readonly EngineEvent[]>("engineevents", { detail: events }));
    }
  }

  private applyStatus(status: NativeResponse): void {
    const active = status.active === true;
    const nextState: AudioContextState | "idle" = active ? "running" : "suspended";
    if (finiteNumber(status.sampleRate) && status.sampleRate >= 8_000 && status.sampleRate <= 384_000) {
      this.currentSampleRate = status.sampleRate;
    }
    if (Number.isSafeInteger(status.routeRevision) && Number(status.routeRevision) !== this.routeRevision) {
      this.routeRevision = Number(status.routeRevision);
      this.dispatchEvent(new Event("devicechange"));
    }
    if (nextState !== this.currentState) {
      this.currentState = nextState;
      this.dispatchEvent(new Event("statechange"));
    }
  }

  private tryRequest(method: string, params: NativeParams): NativeResponse | undefined {
    try {
      return this.request(method, params);
    } catch {
      return undefined;
    }
  }

  private request(method: string, params: NativeParams): NativeResponse {
    const bridge = (window as NativeWindow).MolKeyboardNative;
    if (bridge === undefined) throw new Error("The Android native bridge is unavailable");
    const raw = bridge.dispatch(JSON.stringify({ version: 1, method, params }));
    if (raw.length < 2 || raw.length > MAXIMUM_RESPONSE_CHARS) {
      throw new Error("The Android native bridge returned an invalid response size");
    }
    const response = JSON.parse(raw) as unknown;
    if (!isObject(response) || typeof response.ok !== "boolean") {
      throw new Error("The Android native bridge returned an invalid response");
    }
    if (!response.ok) {
      const message = typeof response.error === "string" ? response.error.slice(0, 256) : "Native request failed";
      throw new Error(message);
    }
    return response;
  }
}
