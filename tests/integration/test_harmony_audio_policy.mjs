// SPDX-License-Identifier: Apache-2.0

import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

if (process.argv.length !== 3) {
  throw new Error('usage: node test_harmony_audio_policy.mjs <AudioPolicy.ets>');
}

const source = await readFile(process.argv[2], 'utf8');
const moduleUrl = `data:text/javascript;base64,${Buffer.from(source).toString('base64')}`;
const { HarmonyAudioPolicy } = await import(moduleUrl);

const policy = new HarmonyAudioPolicy();
assert.equal(policy.userStarted, false);
assert.equal(policy.foreground, true);
assert.equal(policy.shouldContinueInBackground(), false);
assert.equal(policy.shouldRetainOnDestroy(), false);
assert.equal(policy.shouldAttemptRecovery(), false);

policy.userStartRequested();
assert.equal(policy.shouldAttemptRecovery(), true);
policy.enteredBackground();
assert.equal(policy.shouldContinueInBackground(), false);
assert.equal(policy.shouldAttemptRecovery(), false);

policy.playbackStarted();
assert.equal(policy.shouldContinueInBackground(), true);
assert.equal(policy.shouldAttemptRecovery(), true);
policy.continuousTaskStarted();
assert.equal(policy.shouldRetainOnDestroy(), true);
policy.playbackStopped();
assert.equal(policy.shouldContinueInBackground(), false);
assert.equal(policy.shouldRetainOnDestroy(), false);

policy.metronomeEnabledChanged();
assert.equal(policy.shouldContinueInBackground(), false);
policy.transportToggleAccepted();
assert.equal(policy.shouldContinueInBackground(), true);
policy.continuousTaskStopped();
assert.equal(policy.shouldRetainOnDestroy(), false);
policy.transportToggleAccepted();
assert.equal(policy.shouldContinueInBackground(), false);

policy.enteredForeground();
assert.equal(policy.shouldAttemptRecovery(), true);
policy.userStartFailed();
assert.equal(policy.shouldAttemptRecovery(), false);

policy.userStartRequested();
policy.playbackStarted();
policy.transportToggleAccepted();
policy.userStopped();
assert.equal(policy.userStarted, false);
assert.equal(policy.playbackRunning, false);
assert.equal(policy.transportRunning, false);
assert.equal(policy.metronomeEnabled, true);
assert.equal(policy.shouldContinueInBackground(), false);

console.log('HarmonyOS production audio policy simulation passed');
