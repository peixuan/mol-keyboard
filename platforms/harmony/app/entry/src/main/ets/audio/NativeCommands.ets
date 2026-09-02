// SPDX-License-Identifier: Apache-2.0

export class NativeCommands {
  static readonly NOTE_ON: number = 1;
  static readonly NOTE_OFF: number = 2;
  static readonly SUSTAIN: number = 5;
  static readonly ALL_NOTES_OFF: number = 6;
  static readonly ALL_SOUND_OFF: number = 7;
  static readonly SET_MASTER_GAIN: number = 8;
  static readonly SET_PRESET: number = 9;
  static readonly SET_PARAMETER: number = 10;
  static readonly SET_OCTAVE: number = 11;
  static readonly SET_TRANSPOSE: number = 12;
  static readonly SET_SCALE: number = 13;
  static readonly SET_CHORD: number = 14;
  static readonly SET_ARPEGGIATOR: number = 15;
  static readonly SET_TEMPO: number = 16;
  static readonly SET_TIME_SIGNATURE: number = 17;
  static readonly TRANSPORT_START: number = 18;
  static readonly TRANSPORT_STOP: number = 19;
  static readonly RECORD_START: number = 21;
  static readonly RECORD_STOP: number = 22;
  static readonly PLAYBACK_START: number = 23;
  static readonly PLAYBACK_STOP: number = 24;
  static readonly RESET_ENGINE: number = 26;
  static readonly SET_METRONOME: number = 27;
  static readonly SET_PORTAMENTO: number = 28;
}

export class NativeEvents {
  static readonly RECORDING_CHANGED: number = 7;
  static readonly PLAYBACK_CHANGED: number = 14;
}
