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
export const status: (handle: NativeAudioHandle) => NativeAudioStatus;

declare const nativeAudio: {
  create: typeof create;
  start: typeof start;
  stop: typeof stop;
  recover: typeof recover;
  noteOn: typeof noteOn;
  noteOff: typeof noteOff;
  status: typeof status;
};

export default nativeAudio;
