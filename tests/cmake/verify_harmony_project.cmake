# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_app "${MOL_SOURCE_DIR}/platforms/harmony/app")
set(_project_profile "${_app}/build-profile.json5")
set(_module "${_app}/entry/src/main/module.json5")
set(_compat_module "${_app}/entry-openharmony/src/main/module.json5")
set(_compat_profile "${_app}/entry-openharmony/build-profile.json5")
set(_service "${_app}/entry/src/main/ets/audio/AudioService.ets")
set(_audio_policy "${_app}/entry/src/main/ets/audio/AudioPolicy.ets")
set(_page "${_app}/entry/src/main/ets/pages/Index.ets")
set(_native "${MOL_SOURCE_DIR}/platforms/harmony/native/napi_module.cpp")
set(_audio_host "${MOL_SOURCE_DIR}/platforms/harmony/native/oh_audio_host.cpp")
set(_audio_host_sim "${MOL_SOURCE_DIR}/tests/platform/test_harmony_audio_host_sim.cpp")
set(_ohaudio_sim "${MOL_SOURCE_DIR}/tests/platform/harmony_ohaudio_sim.cpp")
set(_napi_bridge_sim "${MOL_SOURCE_DIR}/tests/platform/test_harmony_napi_bridge_sim.cpp")
set(_napi_sim "${MOL_SOURCE_DIR}/tests/platform/harmony_napi_sim.cpp")
set(_audio_policy_sim "${MOL_SOURCE_DIR}/tests/integration/test_harmony_audio_policy.mjs")
set(_tests_cmake "${MOL_SOURCE_DIR}/tests/CMakeLists.txt")
set(_types "${_app}/entry/src/main/cpp/types/libmol_harmony_audio/index.d.ts")
set(_compat_script "${MOL_SOURCE_DIR}/platforms/harmony/build-openharmony-compat.sh")
set(_compat_ps_script "${MOL_SOURCE_DIR}/platforms/harmony/build-openharmony-compat.ps1")

foreach(_required
        "${_app}/AppScope/app.json5"
        "${_project_profile}"
        "${_app}/entry/build-profile.json5"
        "${_app}/entry/src/main/cpp/CMakeLists.txt"
        "${_compat_module}"
        "${_compat_profile}"
        "${_module}"
        "${_service}"
        "${_audio_policy}"
        "${_page}"
        "${_audio_host}"
        "${_audio_host_sim}"
        "${_ohaudio_sim}"
        "${_napi_bridge_sim}"
        "${_napi_sim}"
        "${_audio_policy_sim}"
        "${_tests_cmake}"
        "${_types}"
        "${_compat_script}"
        "${_compat_ps_script}"
        "${_app}/AppScope/resources/base/media/app_icon.png"
        "${_app}/entry/src/main/resources/base/media/startIcon.png")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "HarmonyOS application input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_audio_host_sim}" _audio_host_sim_text)
file(READ "${_ohaudio_sim}" _ohaudio_sim_text)
file(READ "${_napi_bridge_sim}" _napi_bridge_sim_text)
file(READ "${_napi_sim}" _napi_sim_text)
file(READ "${_audio_policy_sim}" _audio_policy_sim_text)
file(READ "${_tests_cmake}" _tests_cmake_text)
foreach(_token
        "reject_fast_latency"
        "change_output_device"
        "AUDIOSTREAM_INTERRUPT_HINT_PAUSE"
        "AUDIOSTREAM_ERROR_SYSTEM"
        "export_recording"
        "load_recording")
  if(NOT _audio_host_sim_text MATCHES "${_token}" AND
     NOT _ohaudio_sim_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS OHAudio simulation is missing ${_token}")
  endif()
endforeach()
foreach(_token
        "Invalid native audio arguments"
        "Invalid native audio handle"
        "pollEvents"
        "exportRecording"
        "loadRecording"
        "routeChanges")
  if(NOT _napi_bridge_sim_text MATCHES "${_token}" AND
     NOT _napi_sim_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS Node-API simulation is missing ${_token}")
  endif()
endforeach()
foreach(_token
        "mol_harmony_audio_host_simulation_tests"
        "mol_harmony_napi_bridge_simulation_tests"
        "mol_harmony_audio_policy_simulation"
        "Node.js is required for the HarmonyOS production audio-policy test"
        [[platforms/harmony/native/oh_audio_host.cpp]]
        [[platforms/harmony/native/napi_module.cpp]]
        [[platform/harmony_napi_sim.cpp]]
        [[platform/harmony_ohaudio_sim.cpp]])
  if(NOT _tests_cmake_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS OHAudio simulation build wiring is missing ${_token}")
  endif()
endforeach()
foreach(_token
        "readFile"
        "process.argv"
        "moduleUrl"
        "shouldContinueInBackground"
        "shouldRetainOnDestroy"
        "shouldAttemptRecovery")
  if(NOT _audio_policy_sim_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS production audio policy simulation is missing ${_token}")
  endif()
endforeach()

file(READ "${_project_profile}" _project_profile_text)
foreach(_token
        [["runtimeOS": "HarmonyOS"]]
        [["runtimeOS": "OpenHarmony"]]
        [["name": "entryOpenHarmony"]]
        [["compatibleSdkVersion": 12]])
  if(NOT _project_profile_text MATCHES "${_token}")
    message(FATAL_ERROR "Harmony project profile is missing ${_token}")
  endif()
endforeach()

file(READ "${_compat_module}" _compat_module_text)
foreach(_token [["name": "entryOpenHarmony"]] [["tablet"]] "audioPlayback")
  if(NOT _compat_module_text MATCHES "${_token}")
    message(FATAL_ERROR "OpenHarmony compatibility module is missing ${_token}")
  endif()
endforeach()

file(READ "${_compat_profile}" _compat_profile_text)
foreach(_token
        [["./src/main/cpp/CMakeLists.txt"]]
        [["arm64-v8a"]]
        [["x86_64"]]
        [["runtimeOS": "OpenHarmony"]])
  if(NOT _compat_profile_text MATCHES "${_token}")
    message(FATAL_ERROR "OpenHarmony compatibility profile is missing ${_token}")
  endif()
endforeach()

file(READ "${_module}" _module_text)
foreach(_token
        "ohos.permission.KEEP_BACKGROUND_RUNNING"
        "audioPlayback"
        "EntryAbility")
  if(NOT _module_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS manifest is missing ${_token}")
  endif()
endforeach()

file(READ "${_service}" _service_text)
foreach(_token
        "import { HarmonyAudioPolicy } from './AudioPolicy'"
        "new HarmonyAudioPolicy"
        "this.policy.shouldContinueInBackground"
        "this.policy.shouldRetainOnDestroy"
        "this.policy.shouldAttemptRecovery"
        "getSessionManager"
        "activateAudioSession"
        "audioSessionDeactivated"
        "BackgroundMode.AUDIO_PLAYBACK"
        "startBackgroundRunning"
        "stopBackgroundRunning"
        "createAVSession"
        "filesDir"
        "renameSync")
  if(NOT _service_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS service is missing ${_token}")
  endif()
endforeach()

file(READ "${_audio_policy}" _audio_policy_text)
foreach(_token
        "export class HarmonyAudioPolicy"
        "userStartRequested"
        "enteredForeground"
        "enteredBackground"
        "transportToggleAccepted"
        "playbackStarted"
        "metronomeEnabledChanged"
        "continuousTaskStarted"
        "shouldContinueInBackground"
        "shouldRetainOnDestroy"
        "shouldAttemptRecovery")
  if(NOT _audio_policy_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS audio policy is missing ${_token}")
  endif()
endforeach()

file(READ "${_page}" _page_text)
string(REGEX MATCHALL "keyCode: KeyCode\\.KEYCODE_[A-Z0-9_]+" _key_bindings "${_page_text}")
list(LENGTH _key_bindings _key_binding_count)
if(NOT _key_binding_count EQUAL 30)
  message(FATAL_ERROR "HarmonyOS UI must expose exactly 30 keyboard note bindings; found ${_key_binding_count}")
endif()
foreach(_token "PRESETS" "SCALES" "CHORDS" "ARPEGGIATORS" "PORTAMENTO_MODES")
  if(NOT _page_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS UI is missing ${_token}")
  endif()
endforeach()
if(NOT _page_text MATCHES "interface SelectOption" OR
   NOT _page_text MATCHES "@State scaleIndex")
  message(FATAL_ERROR "HarmonyOS UI constants must satisfy strict ArkTS typing")
endif()

file(GLOB_RECURSE _arkts_sources "${_app}/entry/src/main/ets/*.ets")
foreach(_source IN LISTS _arkts_sources)
  file(READ "${_source}" _arkts_text)
  if(_arkts_text MATCHES "AudioRenderer|writeData")
    message(FATAL_ERROR "ArkTS must not render PCM: ${_source}")
  endif()
endforeach()

file(READ "${_audio_host}" _audio_host_text)
foreach(_token
        "AUDIOSTREAM_SAMPLE_S16LE"
        "OH_AudioStreamBuilder_SetRendererCallback"
        "kRenderChunkFrames"
        "mol_platform_audio_render_f32")
  if(NOT _audio_host_text MATCHES "${_token}")
    message(FATAL_ERROR "HarmonyOS API 12 audio host is missing ${_token}")
  endif()
endforeach()
foreach(_forbidden
        "AUDIOSTREAM_SAMPLE_F32LE"
        "SetRendererInterruptCallback"
        "SetRendererErrorCallback")
  if(_audio_host_text MATCHES "${_forbidden}")
    message(FATAL_ERROR "HarmonyOS audio host uses a post-API-12 interface: ${_forbidden}")
  endif()
endforeach()

foreach(_script "${_compat_script}" "${_compat_ps_script}")
  file(READ "${_script}" _script_text)
  foreach(_token
          "clean --mode module"
          "product=openharmony"
          "entryOpenHarmony@default"
          "libs/arm64-v8a/libmol_harmony_audio.so"
          "libs/x86_64/libmol_harmony_audio.so"
          "ets/modules.abc"
          "HAP bytecode SHA256")
    if(NOT _script_text MATCHES "${_token}")
      message(FATAL_ERROR "OpenHarmony compatibility build audit is missing ${_token}: ${_script}")
    endif()
  endforeach()
endforeach()

file(READ "${_native}" _native_text)
file(READ "${_types}" _types_text)
foreach(_method
        "submitControl"
        "pollEvents"
        "exportRecording"
        "loadRecording"
        "fastPathActive"
        "latencyFallbackUsed")
  if(NOT _native_text MATCHES "${_method}" OR NOT _types_text MATCHES "${_method}")
    message(FATAL_ERROR "HarmonyOS Node-API surface is missing ${_method}")
  endif()
endforeach()
