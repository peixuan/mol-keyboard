// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "harmony_napi_sim.h"
#include "harmony_ohaudio_sim.h"
#include "mol/mol.h"

namespace {

#define CHECK(condition)                                                           \
  do {                                                                             \
    if (!(condition)) {                                                            \
      std::cerr << "check failed at line " << __LINE__ << ": " #condition << '\n'; \
      return false;                                                                \
    }                                                                              \
  } while (false)

using mol::harmony::test::array_element;
using mol::harmony::test::array_length;
using mol::harmony::test::boolean_value;
using mol::harmony::test::call;
using mol::harmony::test::integer_value;
using mol::harmony::test::make_double;
using mol::harmony::test::make_int32;
using mol::harmony::test::make_int64;
using mol::harmony::test::property;

struct Environment {
  napi_env value = mol::harmony::test::create_environment();
  Environment() = default;
  ~Environment() { mol::harmony::test::destroy_environment(value); }
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;
};

napi_value control(napi_env environment, napi_value exports, napi_value handle,
                   std::int32_t command, std::int32_t integer_0 = 0, double scalar_0 = 0.0) {
  return call(environment, exports, "submitControl",
              {handle, make_int32(environment, command), make_int64(environment, 0),
               make_int32(environment, integer_0), make_int32(environment, 0),
               make_int32(environment, 0), make_int32(environment, 0),
               make_double(environment, scalar_0), make_double(environment, 0.0)});
}

bool render(std::vector<std::int16_t>& audio) {
  return mol::harmony::test::write(audio.data(),
                                   static_cast<std::int32_t>(audio.size() * sizeof(audio[0]))) ==
         AUDIO_DATA_CALLBACK_RESULT_VALID;
}

bool contains_signal(const std::vector<std::int16_t>& audio) {
  return std::any_of(audio.begin(), audio.end(), [](std::int16_t sample) { return sample != 0; });
}

bool has_note_event(napi_value events, std::int64_t gesture, std::int64_t note) {
  const std::size_t length = array_length(events);
  if (length % 5U != 0U) return false;
  for (std::size_t offset = 0U; offset < length; offset += 5U) {
    if (integer_value(array_element(events, offset + 1U)) == gesture &&
        integer_value(array_element(events, offset + 3U)) == note) {
      return true;
    }
  }
  return false;
}

bool test_module_surface_and_validation(napi_env environment, napi_value exports,
                                        napi_value& handle) {
  const std::vector<std::string> methods{
      "create",          "start",         "stop",          "recover",
      "noteOn",          "noteOff",       "submitControl", "pollEvents",
      "exportRecording", "loadRecording", "status",
  };
  CHECK(mol::harmony::test::method_count(exports) == methods.size());
  for (const auto& method : methods) CHECK(mol::harmony::test::has_method(exports, method));

  handle = call(environment, exports, "create", {});
  CHECK(handle != nullptr);
  CHECK(!mol::harmony::test::has_exception(environment));
  CHECK(integer_value(call(environment, exports, "start", {handle})) == AUDIOSTREAM_SUCCESS);

  napi_value status = call(environment, exports, "status", {handle});
  CHECK(status != nullptr);
  CHECK(integer_value(property(status, "sampleRate")) == 48000);
  CHECK(integer_value(property(status, "frameSize")) == 192);
  CHECK(integer_value(property(status, "latencyMode")) == AUDIOSTREAM_LATENCY_MODE_FAST);
  CHECK(boolean_value(property(status, "active")));
  CHECK(!boolean_value(property(status, "needsRestart")));
  CHECK(boolean_value(property(status, "fastPathActive")));
  CHECK(!boolean_value(property(status, "latencyFallbackUsed")));

  CHECK(call(environment, exports, "start", {}) == nullptr);
  CHECK(mol::harmony::test::exception_message(environment) == "Invalid native audio arguments");
  mol::harmony::test::clear_exception(environment);
  CHECK(call(environment, exports, "status",
             {mol::harmony::test::make_boolean(environment, false)}) == nullptr);
  CHECK(mol::harmony::test::exception_message(environment) == "Invalid native audio handle");
  mol::harmony::test::clear_exception(environment);
  CHECK(call(environment, exports, "noteOn",
             {handle, make_int32(environment, 128), make_double(environment, 0.5),
              make_int64(environment, 1)}) == nullptr);
  CHECK(mol::harmony::test::exception_message(environment) == "Invalid note-on command");
  mol::harmony::test::clear_exception(environment);
  CHECK(call(environment, exports, "noteOn",
             {handle, make_int32(environment, 60),
              mol::harmony::test::make_boolean(environment, true), make_int64(environment, 1)}) ==
        nullptr);
  CHECK(mol::harmony::test::exception_message(environment) == "Invalid note-on command");
  mol::harmony::test::clear_exception(environment);
  CHECK(integer_value(call(environment, exports, "noteOn",
                           {handle, make_int32(environment, 60), make_double(environment, 2.0),
                            make_int64(environment, 1)})) == MOL_ERROR_INVALID_ARGUMENT);
  CHECK(!mol::harmony::test::has_exception(environment));
  return true;
}

bool test_events_recording_and_recovery(napi_env environment, napi_value exports,
                                        napi_value handle) {
  std::vector<std::int16_t> audio(512U * 2U, 0);
  CHECK(integer_value(call(environment, exports, "noteOn",
                           {handle, make_int32(environment, 60), make_double(environment, 0.8),
                            make_int64(environment, 77)})) == MOL_OK);
  CHECK(render(audio));
  CHECK(contains_signal(audio));
  napi_value events = call(environment, exports, "pollEvents", {handle});
  CHECK(events != nullptr);
  CHECK(array_length(events) > 0U);
  CHECK(array_length(events) <= 64U * 5U);
  CHECK(has_note_event(events, 77, 60));
  CHECK(integer_value(call(environment, exports, "noteOff",
                           {handle, make_int32(environment, 60), make_int64(environment, 77)})) ==
        MOL_OK);
  CHECK(render(audio));

  CHECK(integer_value(control(environment, exports, handle, MOL_COMMAND_RECORD_START)) == MOL_OK);
  CHECK(integer_value(call(environment, exports, "noteOn",
                           {handle, make_int32(environment, 64), make_double(environment, 0.75),
                            make_int64(environment, 440)})) == MOL_OK);
  CHECK(render(audio));
  CHECK(integer_value(call(environment, exports, "noteOff",
                           {handle, make_int32(environment, 64), make_int64(environment, 440)})) ==
        MOL_OK);
  CHECK(render(audio));
  CHECK(integer_value(control(environment, exports, handle, MOL_COMMAND_RECORD_STOP)) == MOL_OK);
  CHECK(render(audio));

  napi_value exported = call(environment, exports, "exportRecording", {handle});
  const std::vector<std::uint8_t> sequence = mol::harmony::test::arraybuffer_bytes(exported);
  CHECK(!sequence.empty());
  CHECK(sequence.size() <= 2U * 1024U * 1024U);

  napi_value stopped = call(environment, exports, "stop", {handle});
  CHECK(mol::harmony::test::is_undefined(stopped));
  CHECK(integer_value(call(environment, exports, "start", {handle})) == AUDIOSTREAM_SUCCESS);
  napi_value recording = mol::harmony::test::make_arraybuffer(environment, sequence);
  CHECK(integer_value(call(environment, exports, "loadRecording", {handle, recording})) == MOL_OK);
  napi_value empty = mol::harmony::test::make_arraybuffer(environment, {});
  CHECK(call(environment, exports, "loadRecording", {handle, empty}) == nullptr);
  CHECK(mol::harmony::test::exception_message(environment) == "Invalid recording data");
  mol::harmony::test::clear_exception(environment);

  CHECK(integer_value(control(environment, exports, handle, MOL_COMMAND_PLAYBACK_START)) == MOL_OK);
  bool playback_signal = false;
  for (std::uint32_t iteration = 0U; iteration < 8U; ++iteration) {
    std::fill(audio.begin(), audio.end(), static_cast<std::int16_t>(0));
    CHECK(render(audio));
    playback_signal = playback_signal || contains_signal(audio);
  }
  CHECK(playback_signal);

  mol::harmony::test::change_output_device(REASON_NEW_DEVICE_AVAILABLE);
  napi_value status = call(environment, exports, "status", {handle});
  CHECK(!boolean_value(property(status, "active")));
  CHECK(boolean_value(property(status, "needsRestart")));
  CHECK(integer_value(property(status, "routeChanges")) == 1);
  CHECK(integer_value(call(environment, exports, "recover", {handle})) == AUDIOSTREAM_SUCCESS);
  status = call(environment, exports, "status", {handle});
  CHECK(boolean_value(property(status, "active")));
  CHECK(!boolean_value(property(status, "needsRestart")));
  CHECK(integer_value(property(status, "callbackCount")) >= 1);
  CHECK(integer_value(property(status, "renderedFrames")) >= 512);
  return true;
}

}  // namespace

int main() {
  mol::harmony::test::reset();
  Environment environment;
  if (environment.value == nullptr) return 1;
  napi_value exports = mol::harmony::test::initialize_registered_module(environment.value);
  if (exports == nullptr) return 1;
  napi_value handle = nullptr;
  if (!test_module_surface_and_validation(environment.value, exports, handle) ||
      !test_events_recording_and_recovery(environment.value, exports, handle)) {
    return 1;
  }
  std::cout << "HarmonyOS production Node-API bridge simulation passed\n";
  return 0;
}
