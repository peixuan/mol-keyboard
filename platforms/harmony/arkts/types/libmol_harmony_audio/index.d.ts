// SPDX-License-Identifier: Apache-2.0

export type NativeAudioHandle = object;

export interface NativeAudioStatus {
  sampleRate: number;
  frameSize: number;
  latencyMode: number;
  callbackCount: number;
  renderedFrames: number;
  renderFailures: number;
  nonFiniteSamples: number;
  underflowCount: number;
  routeChanges: number;
  interruptions: number;
  lastError: number;
  active: boolean;
  needsRestart: boolean;
  fastPathActive: boolean;
  latencyFallbackUsed: boolean;
}

export const create: () => NativeAudioHandle;
export const start: (handle: NativeAudioHandle) => number;
export const stop: (handle: NativeAudioHandle) => void;
export const recover: (handle: NativeAudioHandle) => number;
export const noteOn: (
  handle: NativeAudioHandle,
  note: number,
  velocity: number,
  gestureId: number,
) => number;
export const noteOff: (
  handle: NativeAudioHandle,
  note: number,
  gestureId: number,
) => number;
export const submitControl: (
  handle: NativeAudioHandle,
  commandType: number,
  gestureId: number,
  integer0: number,
  integer1: number,
  integer2: number,
  integer3: number,
  scalar0: number,
  scalar1: number,
) => number;
export const pollEvents: (handle: NativeAudioHandle) => number[];
export const exportRecording: (handle: NativeAudioHandle) => ArrayBuffer | number;
export const loadRecording: (
  handle: NativeAudioHandle,
  recording: ArrayBuffer,
) => number;
export const status: (handle: NativeAudioHandle) => NativeAudioStatus;

declare const nativeAudio: {
  create: typeof create;
  start: typeof start;
  stop: typeof stop;
  recover: typeof recover;
  noteOn: typeof noteOn;
  noteOff: typeof noteOff;
  submitControl: typeof submitControl;
  pollEvents: typeof pollEvents;
  exportRecording: typeof exportRecording;
  loadRecording: typeof loadRecording;
  status: typeof status;
};

export default nativeAudio;
