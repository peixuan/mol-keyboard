# SPDX-License-Identifier: Apache-2.0
include_guard(GLOBAL)

include(FetchContent)

set(MOL_MINIAUDIO_VERSION "0.11.25")
set(MOL_MINIAUDIO_COMMIT "9634bedb5b5a2ca38c1ee7108a9358a4e233f14d")
set(MOL_MINIAUDIO_ARCHIVE_SHA256
    "1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55")

set(MOL_OBOE_VERSION "1.10.0")
set(MOL_OBOE_COMMIT "a81bb9f87d4105b84b682685d3bfbb5beca371d1")
set(MOL_OBOE_ARCHIVE_SHA256
    "0e4245f8860c4287040a5d76501c588490bcc9cb57614c486c0c201a5dde3e9f")

function(mol_add_miniaudio)
  if(TARGET mol_miniaudio)
    return()
  endif()

  set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_EXTRA_NODES ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_DECODING ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_ENCODING ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_WAV ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_FLAC ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_MP3 ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_RESOURCE_MANAGER ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_NODE_GRAPH ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_ENGINE ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_NO_GENERATION ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_USE_STDINT ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_ONLY_SPECIFIC_BACKENDS ON CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_WASAPI OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_PULSEAUDIO OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_ALSA OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_JACK OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_COREAUDIO OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_ENABLE_NULL ON CACHE BOOL "" FORCE)

  set(_mol_miniaudio_definitions
      MA_NO_DECODING
      MA_NO_ENCODING
      MA_NO_WAV
      MA_NO_FLAC
      MA_NO_MP3
      MA_NO_RESOURCE_MANAGER
      MA_NO_NODE_GRAPH
      MA_NO_ENGINE
      MA_NO_GENERATION
      MA_USE_STDINT
      MA_ENABLE_ONLY_SPECIFIC_BACKENDS
      MA_ENABLE_NULL)
  if(WIN32)
    set(MINIAUDIO_ENABLE_WASAPI ON CACHE BOOL "" FORCE)
    list(APPEND _mol_miniaudio_definitions MA_ENABLE_WASAPI)
  elseif(APPLE)
    set(MINIAUDIO_ENABLE_COREAUDIO ON CACHE BOOL "" FORCE)
    list(APPEND _mol_miniaudio_definitions MA_ENABLE_COREAUDIO)
  elseif(UNIX)
    set(MINIAUDIO_ENABLE_PULSEAUDIO ON CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_ALSA ON CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_JACK ON CACHE BOOL "" FORCE)
    list(APPEND _mol_miniaudio_definitions MA_ENABLE_PULSEAUDIO MA_ENABLE_ALSA
         MA_ENABLE_JACK)
  endif()

  FetchContent_Declare(
    miniaudio
    URL
      "https://github.com/mackron/miniaudio/archive/${MOL_MINIAUDIO_COMMIT}.tar.gz"
    URL_HASH "SHA256=${MOL_MINIAUDIO_ARCHIVE_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  # miniaudio follows the directory-wide BUILD_SHARED_LIBS setting and does not
  # publish a Windows DLL export contract.  It is an implementation detail of
  # the desktop host, so do not let a shared mol_core build turn it into a DLL.
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(miniaudio)

  add_library(mol_miniaudio INTERFACE)
  add_library(mol::miniaudio ALIAS mol_miniaudio)
  target_link_libraries(mol_miniaudio INTERFACE miniaudio)
  target_compile_definitions(mol_miniaudio INTERFACE ${_mol_miniaudio_definitions})
  set_property(TARGET miniaudio PROPERTY FOLDER third_party)
endfunction()

function(mol_add_oboe)
  if(TARGET mol_oboe)
    return()
  endif()
  if(NOT ANDROID)
    message(FATAL_ERROR "Oboe is only available for Android builds")
  endif()

  set(OBOE_DISABLE_CONVERSION OFF CACHE BOOL "" FORCE)
  set(OBOE_DO_NOT_DEFINE_OPENSL_ES_CONSTANTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    oboe
    URL "https://github.com/google/oboe/archive/refs/tags/${MOL_OBOE_VERSION}.tar.gz"
    URL_HASH "SHA256=${MOL_OBOE_ARCHIVE_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(oboe)

  # Oboe's public headers are third-party code. Keep the project's strict warning
  # policy for first-party sources without promoting upstream header warnings to
  # errors in every consumer.
  set_property(TARGET oboe APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                                           "${oboe_SOURCE_DIR}/include")

  add_library(mol_oboe INTERFACE)
  add_library(mol::oboe ALIAS mol_oboe)
  target_link_libraries(mol_oboe INTERFACE oboe)
  set_property(TARGET oboe PROPERTY FOLDER third_party)
endfunction()

function(mol_add_oboe_headers)
  if(TARGET mol_oboe_headers)
    return()
  endif()

  FetchContent_Declare(
    oboe_headers
    URL "https://github.com/google/oboe/archive/refs/tags/${MOL_OBOE_VERSION}.tar.gz"
    URL_HASH "SHA256=${MOL_OBOE_ARCHIVE_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR mol-keyboard-headers-only)
  FetchContent_MakeAvailable(oboe_headers)

  add_library(mol_oboe_headers INTERFACE)
  add_library(mol::oboe_headers ALIAS mol_oboe_headers)
  target_include_directories(mol_oboe_headers SYSTEM INTERFACE
                             "${oboe_headers_SOURCE_DIR}/include")
endfunction()
