// SPDX-License-Identifier: Apache-2.0
import nativeAudio, {
  NativeAudioHandle,
  NativeAudioStatus,
} from 'libmol_harmony_audio.so';

export interface NativeAudioEvent {
  eventType: number;
  gestureId: number;
  frame: number;
  note: number;
  value: number;
}

const EVENT_FIELD_COUNT: number = 5;

export class AudioRuntime {
  private readonly handle: NativeAudioHandle = nativeAudio.create();

  start(): number {
    return nativeAudio.start(this.handle);
  }

  stop(): void {
    nativeAudio.stop(this.handle);
  }

  recover(): number {
    return nativeAudio.recover(this.handle);
  }

  noteOn(note: number, velocity: number, gestureId: number): number {
    return nativeAudio.noteOn(this.handle, note, velocity, gestureId);
  }

  noteOff(note: number, gestureId: number): number {
    return nativeAudio.noteOff(this.handle, note, gestureId);
  }

  submitControl(commandType: number, gestureId: number = 0,
    integer0: number = 0, integer1: number = 0, integer2: number = 0,
    integer3: number = 0, scalar0: number = 0, scalar1: number = 0): number {
    return nativeAudio.submitControl(this.handle, commandType, gestureId,
      integer0, integer1, integer2, integer3, scalar0, scalar1);
  }

  pollEvents(): NativeAudioEvent[] {
    const fields: number[] = nativeAudio.pollEvents(this.handle);
    const events: NativeAudioEvent[] = [];
    for (let offset: number = 0; offset + EVENT_FIELD_COUNT <= fields.length;
      offset += EVENT_FIELD_COUNT) {
      events.push({
        eventType: fields[offset],
        gestureId: fields[offset + 1],
        frame: fields[offset + 2],
        note: fields[offset + 3],
        value: fields[offset + 4],
      });
    }
    return events;
  }

  exportRecording(): ArrayBuffer | number {
    return nativeAudio.exportRecording(this.handle);
  }

  loadRecording(recording: ArrayBuffer): number {
    return nativeAudio.loadRecording(this.handle, recording);
  }

  status(): NativeAudioStatus {
    return nativeAudio.status(this.handle);
  }
}
