# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_app "${MOL_SOURCE_DIR}/platforms/harmony/app")
set(_module "${_app}/entry/src/main/module.json5")
set(_service "${_app}/entry/src/main/ets/audio/AudioService.ets")
set(_page "${_app}/entry/src/main/ets/pages/Index.ets")
set(_native "${MOL_SOURCE_DIR}/platforms/harmony/native/napi_module.cpp")
set(_types "${_app}/entry/src/main/cpp/types/libmol_harmony_audio/index.d.ts")

foreach(_required
        "${_app}/AppScope/app.json5"
        "${_app}/build-profile.json5"
        "${_app}/entry/build-profile.json5"
        "${_app}/entry/src/main/cpp/CMakeLists.txt"
        "${_module}"
        "${_service}"
        "${_page}"
        "${_types}"
        "${_app}/AppScope/resources/base/media/app_icon.png"
        "${_app}/entry/src/main/resources/base/media/startIcon.png")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "HarmonyOS application input is missing: ${_required}")
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

file(GLOB_RECURSE _arkts_sources "${_app}/entry/src/main/ets/*.ets")
foreach(_source IN LISTS _arkts_sources)
  file(READ "${_source}" _arkts_text)
  if(_arkts_text MATCHES "AudioRenderer|writeData")
    message(FATAL_ERROR "ArkTS must not render PCM: ${_source}")
  endif()
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
