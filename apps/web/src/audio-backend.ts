export interface AudioBackend extends EventTarget {
  readonly state: AudioContextState | "idle";
  readonly sampleRate: number | undefined;
  readonly commandTransport:
    | "message-port"
    | "shared-array-buffer"
    | "websocket-jsonrpc"
    | "native-bridge";
  readonly droppedCommandCount: number;
  start(): Promise<void>;
  close(): Promise<void>;
  noteOn(note: number, velocity: number, gestureId: number): void;
  noteOff(gestureId: number): void;
  allNotesOff(): void;
  setMasterGain(value: number): Promise<boolean>;
  setSustain(value: number): Promise<boolean>;
  setOctave(value: number): Promise<boolean>;
  setTranspose(value: number): Promise<boolean>;
  setPreset(value: number, hardSwitch?: boolean): Promise<boolean>;
  setParameter(parameter: number, value: number): Promise<boolean>;
  setScale(scale: number, tonic: number, mapping: number): Promise<boolean>;
  setChord(value: number): Promise<boolean>;
  setArpeggiator(
    mode: number,
    rate: number,
    gate: number,
    octaves: number,
    seed: number,
  ): Promise<boolean>;
  setTempo(value: number): Promise<boolean>;
  setTimeSignature(numerator: number, denominator: number): Promise<boolean>;
  setMetronome(enabled: boolean, level: number): Promise<boolean>;
  setPortamento(mode: number, timeMs: number): Promise<boolean>;
  action(action: string): Promise<boolean>;
  exportRecording(): Promise<Uint8Array | undefined>;
  loadRecording(bytes: Uint8Array): Promise<boolean>;
}
