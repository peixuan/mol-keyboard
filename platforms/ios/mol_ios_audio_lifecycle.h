// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_PLATFORMS_IOS_AUDIO_LIFECYCLE_H_
#define MOL_PLATFORMS_IOS_AUDIO_LIFECYCLE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mol_ios_audio_action {
  MOL_IOS_AUDIO_ACTION_NONE = 0u,
  MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF = 1u << 0u,
  MOL_IOS_AUDIO_ACTION_STOP = 1u << 1u,
  MOL_IOS_AUDIO_ACTION_RESTART = 1u << 2u,
  MOL_IOS_AUDIO_ACTION_RESTORE_STATE = 1u << 3u,
} mol_ios_audio_action_t;

typedef struct mol_ios_audio_lifecycle {
  uint64_t route_revision;
  bool user_started;
  bool ui_foreground;
  bool transport_running;
  bool playback_running;
  bool metronome_enabled;
} mol_ios_audio_lifecycle_t;

void mol_ios_audio_lifecycle_init(mol_ios_audio_lifecycle_t* lifecycle);
uint32_t mol_ios_audio_lifecycle_start_succeeded(mol_ios_audio_lifecycle_t* lifecycle);
void mol_ios_audio_lifecycle_start_failed(mol_ios_audio_lifecycle_t* lifecycle);
void mol_ios_audio_lifecycle_stop(mol_ios_audio_lifecycle_t* lifecycle);
void mol_ios_audio_lifecycle_did_become_active(mol_ios_audio_lifecycle_t* lifecycle);
uint32_t mol_ios_audio_lifecycle_will_resign_active(mol_ios_audio_lifecycle_t* lifecycle,
                                                    bool host_active);
uint32_t mol_ios_audio_lifecycle_did_enter_background(mol_ios_audio_lifecycle_t* lifecycle,
                                                      bool host_active);
uint32_t mol_ios_audio_lifecycle_command_submitted(mol_ios_audio_lifecycle_t* lifecycle,
                                                   uint32_t command_type, int32_t integer_0);
uint32_t mol_ios_audio_lifecycle_playback_changed(mol_ios_audio_lifecycle_t* lifecycle,
                                                  bool running);
uint32_t mol_ios_audio_lifecycle_host_restarted(mol_ios_audio_lifecycle_t* lifecycle);
uint32_t mol_ios_audio_lifecycle_media_services_reset(mol_ios_audio_lifecycle_t* lifecycle);
uint32_t mol_ios_audio_lifecycle_restart_completed(mol_ios_audio_lifecycle_t* lifecycle,
                                                   bool succeeded);
bool mol_ios_audio_lifecycle_allows_background(const mol_ios_audio_lifecycle_t* lifecycle);

#ifdef __cplusplus
}
#endif

#endif  // MOL_PLATFORMS_IOS_AUDIO_LIFECYCLE_H_
