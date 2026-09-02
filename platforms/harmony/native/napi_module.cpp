// SPDX-License-Identifier: Apache-2.0
#include <napi/native_api.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

#include "oh_audio_host.h"

namespace {

using AudioHost = mol::harmony::AudioHost;

constexpr std::size_t kMaximumRecordingBytes = 2U * 1024U * 1024U;
constexpr std::uint32_t kMaximumEvents = 64U;
constexpr std::uint32_t kEventFieldCount = 5U;

napi_value undefined_value(napi_env environment) {
  napi_value result = nullptr;
  (void)napi_get_undefined(environment, &result);
  return result;
}

bool get_arguments(napi_env environment, napi_callback_info info, std::size_t expected,
                   napi_value* arguments) {
  std::size_t argument_count = expected + 1U;
  if (napi_get_cb_info(environment, info, &argument_count, arguments, nullptr, nullptr) !=
          napi_ok ||
      argument_count != expected) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid native audio arguments");
    return false;
  }
  return true;
}

AudioHost* get_host(napi_env environment, napi_value value) {
  void* external = nullptr;
  if (napi_get_value_external(environment, value, &external) != napi_ok || external == nullptr) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid native audio handle");
    return nullptr;
  }
  return static_cast<AudioHost*>(external);
}

napi_value int32_value(napi_env environment, std::int32_t value) {
  napi_value result = nullptr;
  return napi_create_int32(environment, value, &result) == napi_ok ? result : nullptr;
}

napi_value int64_value(napi_env environment, std::int64_t value) {
  napi_value result = nullptr;
  return napi_create_int64(environment, value, &result) == napi_ok ? result : nullptr;
}

napi_value bool_value(napi_env environment, bool value) {
  napi_value result = nullptr;
  return napi_get_boolean(environment, value, &result) == napi_ok ? result : nullptr;
}

void set_property(napi_env environment, napi_value object, const char* name, napi_value value) {
  if (value != nullptr) {
    (void)napi_set_named_property(environment, object, name, value);
  }
}

void finalize_host(napi_env, void* data, void*) { delete static_cast<AudioHost*>(data); }

napi_value create(napi_env environment, napi_callback_info info) {
  napi_value arguments[1]{};
  if (!get_arguments(environment, info, 0U, arguments)) {
    return nullptr;
  }
  auto* host = new (std::nothrow) AudioHost();
  if (host == nullptr) {
    (void)napi_throw_error(environment, nullptr, "Unable to allocate native audio runtime");
    return nullptr;
  }
  napi_value result = nullptr;
  if (napi_create_external(environment, host, finalize_host, nullptr, &result) != napi_ok) {
    delete host;
    return nullptr;
  }
  return result;
}

napi_value start(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  return host == nullptr ? nullptr : int32_value(environment, host->start());
}

napi_value stop(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  if (host == nullptr) {
    return nullptr;
  }
  host->stop();
  return undefined_value(environment);
}

napi_value recover(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  return host == nullptr ? nullptr : int32_value(environment, host->recover());
}

napi_value note_on(napi_env environment, napi_callback_info info) {
  napi_value arguments[5]{};
  if (!get_arguments(environment, info, 4U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  std::int32_t note = 0;
  double velocity = 0.0;
  std::int64_t gesture_id = 0;
  if (host == nullptr || napi_get_value_int32(environment, arguments[1], &note) != napi_ok ||
      napi_get_value_double(environment, arguments[2], &velocity) != napi_ok ||
      napi_get_value_int64(environment, arguments[3], &gesture_id) != napi_ok || note < 0 ||
      note > 127 || gesture_id < 0) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid note-on command");
    return nullptr;
  }
  return int32_value(environment,
                     host->note_on(static_cast<std::uint8_t>(note), static_cast<float>(velocity),
                                   static_cast<std::uint64_t>(gesture_id)));
}

napi_value note_off(napi_env environment, napi_callback_info info) {
  napi_value arguments[4]{};
  if (!get_arguments(environment, info, 3U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  std::int32_t note = 0;
  std::int64_t gesture_id = 0;
  if (host == nullptr || napi_get_value_int32(environment, arguments[1], &note) != napi_ok ||
      napi_get_value_int64(environment, arguments[2], &gesture_id) != napi_ok || note < 0 ||
      note > 127 || gesture_id < 0) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid note-off command");
    return nullptr;
  }
  return int32_value(environment, host->note_off(static_cast<std::uint8_t>(note),
                                                 static_cast<std::uint64_t>(gesture_id)));
}

napi_value submit_control(napi_env environment, napi_callback_info info) {
  napi_value arguments[10]{};
  if (!get_arguments(environment, info, 9U, arguments)) return nullptr;
  AudioHost* host = get_host(environment, arguments[0]);
  std::int32_t command_type = 0;
  std::int64_t gesture_id = 0;
  std::int32_t integers[4]{};
  double scalars[2]{};
  if (host == nullptr ||
      napi_get_value_int32(environment, arguments[1], &command_type) != napi_ok ||
      napi_get_value_int64(environment, arguments[2], &gesture_id) != napi_ok ||
      napi_get_value_int32(environment, arguments[3], &integers[0]) != napi_ok ||
      napi_get_value_int32(environment, arguments[4], &integers[1]) != napi_ok ||
      napi_get_value_int32(environment, arguments[5], &integers[2]) != napi_ok ||
      napi_get_value_int32(environment, arguments[6], &integers[3]) != napi_ok ||
      napi_get_value_double(environment, arguments[7], &scalars[0]) != napi_ok ||
      napi_get_value_double(environment, arguments[8], &scalars[1]) != napi_ok ||
      command_type < 0 || gesture_id < 0) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid native control command");
    return nullptr;
  }
  return int32_value(environment, host->submit_control(static_cast<std::uint32_t>(command_type),
                                                       static_cast<std::uint64_t>(gesture_id),
                                                       integers[0], integers[1], integers[2],
                                                       integers[3], static_cast<float>(scalars[0]),
                                                       static_cast<float>(scalars[1])));
}

napi_value poll_events(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) return nullptr;
  AudioHost* host = get_host(environment, arguments[0]);
  if (host == nullptr) return nullptr;
  mol_event_t events[kMaximumEvents]{};
  const std::uint32_t count = host->poll_events(events, kMaximumEvents);
  napi_value result = nullptr;
  if (napi_create_array_with_length(environment, count * kEventFieldCount, &result) != napi_ok) {
    return nullptr;
  }
  for (std::uint32_t index = 0U; index < count; ++index) {
    const mol_event_t& event = events[index];
    const std::int64_t values[kEventFieldCount] = {
        static_cast<std::int64_t>(event.event_type),
        static_cast<std::int64_t>(event.gesture_id),
        static_cast<std::int64_t>(event.frame),
        static_cast<std::int64_t>(event.payload[MOL_EVENT_PAYLOAD_NOTE]),
        static_cast<std::int64_t>(event.payload[0]),
    };
    for (std::uint32_t field = 0U; field < kEventFieldCount; ++field) {
      napi_value value = int64_value(environment, values[field]);
      if (value == nullptr ||
          napi_set_element(environment, result, index * kEventFieldCount + field, value) !=
              napi_ok) {
        return nullptr;
      }
    }
  }
  return result;
}

napi_value export_recording(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) return nullptr;
  AudioHost* host = get_host(environment, arguments[0]);
  if (host == nullptr) return nullptr;
  const std::unique_ptr<std::uint8_t[]> bytes{new (std::nothrow)
                                                  std::uint8_t[kMaximumRecordingBytes]};
  if (!bytes) {
    (void)napi_throw_error(environment, nullptr, "Unable to allocate recording export");
    return nullptr;
  }
  std::size_t size = 0U;
  const mol_result_t export_result =
      host->export_recording(bytes.get(), kMaximumRecordingBytes, &size);
  if (export_result != MOL_OK || size == 0U || size > kMaximumRecordingBytes) {
    return int32_value(environment, export_result);
  }
  void* destination = nullptr;
  napi_value result = nullptr;
  if (napi_create_arraybuffer(environment, size, &destination, &result) != napi_ok ||
      destination == nullptr) {
    return nullptr;
  }
  std::memcpy(destination, bytes.get(), size);
  return result;
}

napi_value load_recording(napi_env environment, napi_callback_info info) {
  napi_value arguments[3]{};
  if (!get_arguments(environment, info, 2U, arguments)) return nullptr;
  AudioHost* host = get_host(environment, arguments[0]);
  void* bytes = nullptr;
  std::size_t size = 0U;
  if (host == nullptr ||
      napi_get_arraybuffer_info(environment, arguments[1], &bytes, &size) != napi_ok ||
      bytes == nullptr || size == 0U || size > kMaximumRecordingBytes) {
    (void)napi_throw_type_error(environment, nullptr, "Invalid recording data");
    return nullptr;
  }
  return int32_value(environment,
                     host->load_recording(static_cast<const std::uint8_t*>(bytes), size));
}

napi_value status(napi_env environment, napi_callback_info info) {
  napi_value arguments[2]{};
  if (!get_arguments(environment, info, 1U, arguments)) {
    return nullptr;
  }
  AudioHost* host = get_host(environment, arguments[0]);
  if (host == nullptr) {
    return nullptr;
  }
  const mol::harmony::AudioStatus snapshot = host->status();
  napi_value result = nullptr;
  if (napi_create_object(environment, &result) != napi_ok) {
    return nullptr;
  }
  set_property(environment, result, "sampleRate", int32_value(environment, snapshot.sample_rate));
  set_property(environment, result, "frameSize", int32_value(environment, snapshot.frame_size));
  set_property(environment, result, "latencyMode", int32_value(environment, snapshot.latency_mode));
  set_property(environment, result, "callbackCount",
               int64_value(environment, static_cast<std::int64_t>(snapshot.callback_count)));
  set_property(environment, result, "renderedFrames",
               int64_value(environment, static_cast<std::int64_t>(snapshot.rendered_frames)));
  set_property(environment, result, "renderFailures",
               int32_value(environment, static_cast<std::int32_t>(snapshot.render_failures)));
  set_property(environment, result, "nonFiniteSamples",
               int32_value(environment, static_cast<std::int32_t>(snapshot.non_finite_samples)));
  set_property(environment, result, "underflowCount",
               int32_value(environment, static_cast<std::int32_t>(snapshot.underflow_count)));
  set_property(environment, result, "routeChanges",
               int32_value(environment, static_cast<std::int32_t>(snapshot.route_changes)));
  set_property(environment, result, "interruptions",
               int32_value(environment, static_cast<std::int32_t>(snapshot.interruptions)));
  set_property(environment, result, "lastError", int32_value(environment, snapshot.last_error));
  set_property(environment, result, "active", bool_value(environment, snapshot.active));
  set_property(environment, result, "needsRestart",
               bool_value(environment, snapshot.needs_restart));
  set_property(environment, result, "fastPathActive",
               bool_value(environment, snapshot.fast_path_active));
  set_property(environment, result, "latencyFallbackUsed",
               bool_value(environment, snapshot.latency_fallback_used));
  return result;
}

napi_value initialize(napi_env environment, napi_value exports) {
  const napi_property_descriptor properties[] = {
      {"create", nullptr, create, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"start", nullptr, start, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stop", nullptr, stop, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"recover", nullptr, recover, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"noteOn", nullptr, note_on, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"noteOff", nullptr, note_off, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"submitControl", nullptr, submit_control, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"pollEvents", nullptr, poll_events, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"exportRecording", nullptr, export_recording, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"loadRecording", nullptr, load_recording, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"status", nullptr, status, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  (void)napi_define_properties(environment, exports, sizeof(properties) / sizeof(properties[0]),
                               properties);
  return exports;
}

}  // namespace

#if defined(__clang__) || defined(__GNUC__)
#define MOL_HARMONY_CONSTRUCTOR __attribute__((constructor))
#else
#define MOL_HARMONY_CONSTRUCTOR
#endif

extern "C" MOL_HARMONY_CONSTRUCTOR void register_mol_harmony_audio() {
  static napi_module module{};
  module.nm_version = 1;
  module.nm_register_func = initialize;
  module.nm_modname = "mol_harmony_audio";
  napi_module_register(&module);
}
