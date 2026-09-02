export const SHARED_RING_CAPACITY = 256;
export const SHARED_RING_HEADER_WORDS = 4;
export const SHARED_RING_EVENT_WORDS = 4;

const WRITE_INDEX = 0;
const READ_INDEX = 1;
const DROPPED_COUNT = 2;

export const SharedEventType = {
  NoteOn: 1,
  NoteOff: 2,
  AllNotesOff: 3,
} as const;

export class SharedCommandRing {
  readonly buffer: SharedArrayBuffer;
  readonly capacity: number;
  private readonly words: Int32Array;
  private readonly floats: Float32Array;

  constructor(buffer: SharedArrayBuffer, capacity: number) {
    const requiredWords = SHARED_RING_HEADER_WORDS + capacity * SHARED_RING_EVENT_WORDS;
    if (!Number.isInteger(capacity) || capacity < 2 || buffer.byteLength !== requiredWords * 4) {
      throw new Error("Invalid shared command ring storage");
    }
    this.buffer = buffer;
    this.capacity = capacity;
    this.words = new Int32Array(buffer);
    this.floats = new Float32Array(buffer);
  }

  static create(capacity = SHARED_RING_CAPACITY): SharedCommandRing | undefined {
    if (!window.crossOriginIsolated || typeof SharedArrayBuffer === "undefined") return undefined;
    const words = SHARED_RING_HEADER_WORDS + capacity * SHARED_RING_EVENT_WORDS;
    return new SharedCommandRing(new SharedArrayBuffer(words * 4), capacity);
  }

  get droppedCount(): number {
    return Atomics.load(this.words, DROPPED_COUNT) >>> 0;
  }

  noteOn(note: number, velocity: number, gestureId: number): boolean {
    return this.write(SharedEventType.NoteOn, note, velocity, gestureId);
  }

  noteOff(gestureId: number): boolean {
    return this.write(SharedEventType.NoteOff, 0, 0, gestureId);
  }

  allNotesOff(): boolean {
    return this.write(SharedEventType.AllNotesOff, 0, 0, 0);
  }

  private write(type: number, note: number, velocity: number, gestureId: number): boolean {
    const writeIndex = Atomics.load(this.words, WRITE_INDEX);
    const readIndex = Atomics.load(this.words, READ_INDEX);
    const nextIndex = (writeIndex + 1) % this.capacity;
    if (nextIndex === readIndex) {
      Atomics.add(this.words, DROPPED_COUNT, 1);
      return false;
    }
    const offset = SHARED_RING_HEADER_WORDS + writeIndex * SHARED_RING_EVENT_WORDS;
    this.words[offset] = type;
    this.words[offset + 1] = note;
    this.floats[offset + 2] = velocity;
    this.words[offset + 3] = gestureId;
    Atomics.store(this.words, WRITE_INDEX, nextIndex);
    return true;
  }
}
