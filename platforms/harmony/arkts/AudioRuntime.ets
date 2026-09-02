// SPDX-License-Identifier: Apache-2.0
import nativeAudio, {
  NativeAudioHandle,
  NativeAudioStatus,
} from 'libmol_harmony_audio.so';

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

  status(): NativeAudioStatus {
    return nativeAudio.status(this.handle);
  }
}
