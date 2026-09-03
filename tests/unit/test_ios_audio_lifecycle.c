// SPDX-License-Identifier: Apache-2.0
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "mol/command.h"
#include "mol_ios_audio_lifecycle.h"

#define CHECK(condition)                                                              \
  do {                                                                                \
    if (!(condition)) {                                                               \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
      return 1;                                                                       \
    }                                                                                 \
  } while (0)

static int test_idle_background_stops(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  CHECK(lifecycle.ui_foreground);
  CHECK(!lifecycle.user_started);
  CHECK(mol_ios_audio_lifecycle_start_succeeded(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTORE_STATE);
  CHECK(lifecycle.user_started);
  CHECK(mol_ios_audio_lifecycle_will_resign_active(&lifecycle, true) ==
        MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF);
  CHECK(!lifecycle.ui_foreground);
  CHECK(mol_ios_audio_lifecycle_did_enter_background(&lifecycle, true) ==
        (MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF | MOL_IOS_AUDIO_ACTION_STOP));
  CHECK(!lifecycle.user_started);
  CHECK(!lifecycle.transport_running);
  CHECK(!lifecycle.playback_running);
  CHECK(!lifecycle.metronome_enabled);
  return 0;
}

static int test_playback_background_lifecycle(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  CHECK(mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_PLAYBACK_START, 0) ==
        MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_allows_background(&lifecycle));
  CHECK(mol_ios_audio_lifecycle_did_enter_background(&lifecycle, true) ==
        MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF);
  CHECK(lifecycle.user_started);
  CHECK(mol_ios_audio_lifecycle_playback_changed(&lifecycle, false) == MOL_IOS_AUDIO_ACTION_STOP);
  CHECK(!lifecycle.user_started);
  return 0;
}

static int test_metronome_requires_transport(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_SET_METRONOME, 1);
  CHECK(!mol_ios_audio_lifecycle_allows_background(&lifecycle));
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_TRANSPORT_START, 0);
  CHECK(mol_ios_audio_lifecycle_allows_background(&lifecycle));
  CHECK(mol_ios_audio_lifecycle_did_enter_background(&lifecycle, true) ==
        MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF);
  CHECK(mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_TRANSPORT_STOP, 0) ==
        MOL_IOS_AUDIO_ACTION_STOP);
  CHECK(!lifecycle.user_started);

  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_SET_METRONOME, 1);
  CHECK(mol_ios_audio_lifecycle_did_enter_background(&lifecycle, false) ==
        MOL_IOS_AUDIO_ACTION_STOP);
  return 0;
}

static int test_reset_and_playback_events(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_PLAYBACK_START, 0);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_SET_METRONOME, 1);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_TRANSPORT_START, 0);
  CHECK(mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_RESET_ENGINE, 0) ==
        MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(!lifecycle.playback_running);
  CHECK(!lifecycle.metronome_enabled);
  CHECK(!lifecycle.transport_running);

  CHECK(mol_ios_audio_lifecycle_playback_changed(&lifecycle, true) == MOL_IOS_AUDIO_ACTION_NONE);
  (void)mol_ios_audio_lifecycle_did_enter_background(&lifecycle, true);
  CHECK(mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_RESET_ENGINE, 0) ==
        MOL_IOS_AUDIO_ACTION_STOP);
  CHECK(!lifecycle.user_started);
  return 0;
}

static int test_restart_and_media_reset(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  CHECK(mol_ios_audio_lifecycle_host_restarted(&lifecycle) == MOL_IOS_AUDIO_ACTION_NONE);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  CHECK(mol_ios_audio_lifecycle_host_restarted(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTORE_STATE);
  CHECK(lifecycle.route_revision == 1u);
  CHECK(mol_ios_audio_lifecycle_media_services_reset(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTART);
  CHECK(lifecycle.route_revision == 2u);
  CHECK(mol_ios_audio_lifecycle_restart_completed(&lifecycle, true) ==
        MOL_IOS_AUDIO_ACTION_RESTORE_STATE);
  CHECK(lifecycle.user_started);
  CHECK(mol_ios_audio_lifecycle_media_services_reset(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTART);
  CHECK(mol_ios_audio_lifecycle_restart_completed(&lifecycle, false) == MOL_IOS_AUDIO_ACTION_STOP);
  CHECK(!lifecycle.user_started);

  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  (void)mol_ios_audio_lifecycle_command_submitted(&lifecycle, MOL_COMMAND_PLAYBACK_START, 0);
  (void)mol_ios_audio_lifecycle_did_enter_background(&lifecycle, true);
  CHECK(mol_ios_audio_lifecycle_media_services_reset(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTART);
  CHECK(mol_ios_audio_lifecycle_restart_completed(&lifecycle, true) ==
        MOL_IOS_AUDIO_ACTION_RESTORE_STATE);
  CHECK(lifecycle.playback_running);
  CHECK(lifecycle.user_started);
  return 0;
}

static int test_bounds_and_null_safety(void) {
  mol_ios_audio_lifecycle_t lifecycle;
  mol_ios_audio_lifecycle_init(&lifecycle);
  (void)mol_ios_audio_lifecycle_start_succeeded(&lifecycle);
  lifecycle.route_revision = UINT64_MAX;
  CHECK(mol_ios_audio_lifecycle_host_restarted(&lifecycle) == MOL_IOS_AUDIO_ACTION_RESTORE_STATE);
  CHECK(lifecycle.route_revision == UINT64_MAX);
  CHECK(mol_ios_audio_lifecycle_command_submitted(&lifecycle, UINT32_MAX, 0) ==
        MOL_IOS_AUDIO_ACTION_NONE);
  mol_ios_audio_lifecycle_start_failed(&lifecycle);
  CHECK(!lifecycle.user_started);

  mol_ios_audio_lifecycle_init(NULL);
  mol_ios_audio_lifecycle_start_failed(NULL);
  mol_ios_audio_lifecycle_stop(NULL);
  mol_ios_audio_lifecycle_did_become_active(NULL);
  CHECK(mol_ios_audio_lifecycle_start_succeeded(NULL) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_will_resign_active(NULL, true) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_did_enter_background(NULL, true) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_command_submitted(NULL, MOL_COMMAND_PLAYBACK_START, 0) ==
        MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_playback_changed(NULL, true) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_host_restarted(NULL) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_media_services_reset(NULL) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(mol_ios_audio_lifecycle_restart_completed(NULL, true) == MOL_IOS_AUDIO_ACTION_NONE);
  CHECK(!mol_ios_audio_lifecycle_allows_background(NULL));
  return 0;
}

int main(void) {
  CHECK(test_idle_background_stops() == 0);
  CHECK(test_playback_background_lifecycle() == 0);
  CHECK(test_metronome_requires_transport() == 0);
  CHECK(test_reset_and_playback_events() == 0);
  CHECK(test_restart_and_media_reset() == 0);
  CHECK(test_bounds_and_null_safety() == 0);
  return 0;
}
