// SPDX-License-Identifier: Apache-2.0
#include <array>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "audio_runtime.hpp"
#include "json.hpp"
#include "local_ipc.hpp"
#include "loopback_websocket.hpp"
#include "service_backend.hpp"
#include "service_paths.hpp"
#include "service_rpc.hpp"

namespace {

std::atomic<bool> stop_requested{false};

struct Options {
  bool help = false;
  bool null_backend = false;
  bool version = false;
  std::string device_id = "default";
  std::filesystem::path state_directory = molcontrol::default_service_state_directory();
  std::string endpoint;
  bool websocket_enabled = false;
  std::uint16_t websocket_port = 0u;
  std::vector<std::string> web_origins;
};

void print_usage(const char* executable) {
  std::printf(
      "Usage: %s [--null-backend] [--device-id ID] [--state-dir PATH]\n"
      "          [--endpoint LOCAL_ENDPOINT] [--websocket-port PORT]\n"
      "          [--web-origin ORIGIN]... [--version] [--help]\n"
      "\n"
      "Runs in the foreground as the current user. Use platform user-service files\n"
      "for background startup. WebSocket control is disabled unless a port and at\n"
      "least one exact allowed Origin are supplied; it only binds 127.0.0.1.\n",
      executable);
}

bool parse_port(const char* source, std::uint16_t& port) {
  if (source == nullptr || source[0] == '\0') return false;
  char* end = nullptr;
  const unsigned long value = std::strtoul(source, &end, 10);
  if (end == source || *end != '\0' || value > std::numeric_limits<std::uint16_t>::max())
    return false;
  port = static_cast<std::uint16_t>(value);
  return true;
}

bool parse_options(int argc, char** argv, Options& options) {
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help") {
      options.help = true;
    } else if (argument == "--version") {
      options.version = true;
    } else if (argument == "--null-backend") {
      options.null_backend = true;
    } else if (argument == "--device-id" && index + 1 < argc) {
      options.device_id = argv[++index];
    } else if (argument == "--state-dir" && index + 1 < argc) {
      options.state_directory = argv[++index];
    } else if (argument == "--endpoint" && index + 1 < argc) {
      options.endpoint = argv[++index];
    } else if (argument == "--websocket-port" && index + 1 < argc) {
      options.websocket_enabled = parse_port(argv[++index], options.websocket_port);
      if (!options.websocket_enabled) {
        std::fprintf(stderr, "Invalid WebSocket port\n");
        return false;
      }
    } else if (argument == "--web-origin" && index + 1 < argc) {
      options.web_origins.emplace_back(argv[++index]);
    } else {
      std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argument.c_str());
      return false;
    }
  }
  if (!options.web_origins.empty() && !options.websocket_enabled) {
    std::fprintf(stderr, "--web-origin requires --websocket-port\n");
    return false;
  }
  if (options.websocket_enabled && options.web_origins.empty()) {
    std::fprintf(stderr, "--websocket-port requires at least one --web-origin\n");
    return false;
  }
  return true;
}

std::string poll_event_notification(molkeyboardd::AudioRuntime& runtime) {
  std::array<mol_event_t, 64u> events{};
  const std::size_t count = runtime.poll_events(events.data(), events.size());
  if (count == 0u) return {};
  molseq::Json::Array output;
  output.reserve(count);
  for (std::size_t index = 0u; index < count; ++index) {
    const mol_event_t& event = events[index];
    molseq::Json::Object item;
    item["detail"] = molseq::Json::number(event.payload[0]);
    item["frame"] = molseq::Json::number(event.frame);
    item["gesture"] = molseq::Json::number(event.gesture_id);
    item["note"] = molseq::Json::number(event.payload[MOL_EVENT_PAYLOAD_NOTE]);
    item["source"] = molseq::Json::number(event.source_id);
    item["type"] = molseq::Json::number(event.event_type);
    output.push_back(molseq::Json::object_value(std::move(item)));
  }
  molseq::Json::Object params;
  params["events"] = molseq::Json::array_value(std::move(output));
  molseq::Json::Object notification;
  notification["jsonrpc"] = molseq::Json::string("2.0");
  notification["method"] = molseq::Json::string("engine.events");
  notification["params"] = molseq::Json::object_value(std::move(params));
  return molseq::write_json(molseq::Json::object_value(std::move(notification)));
}

#if defined(_WIN32)
BOOL WINAPI console_handler(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT ||
      event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT) {
    stop_requested.store(true, std::memory_order_release);
    return TRUE;
  }
  return FALSE;
}
#else
void signal_handler(int signal_value) {
  if (signal_value == SIGINT || signal_value == SIGTERM)
    stop_requested.store(true, std::memory_order_release);
}
#endif

bool install_signal_handlers() {
#if defined(_WIN32)
  return SetConsoleCtrlHandler(console_handler, TRUE) != FALSE;
#else
  return std::signal(SIGINT, signal_handler) != SIG_ERR &&
         std::signal(SIGTERM, signal_handler) != SIG_ERR;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse_options(argc, argv, options)) return 2;
  if (options.help) {
    print_usage(argv[0]);
    return 0;
  }
  if (options.version) {
    std::printf("mol-keyboardd %s (API %u)\n", mol_get_version_string(), mol_get_api_version());
    return 0;
  }
  if (!install_signal_handlers()) {
    std::fprintf(stderr, "Could not install process stop handlers\n");
    return 1;
  }

  try {
    options.state_directory = std::filesystem::absolute(options.state_directory).lexically_normal();
    if (options.endpoint.empty())
      options.endpoint = molcontrol::default_local_ipc_endpoint(options.state_directory);

    molkeyboardd::AudioRuntime runtime;
    std::string audio_message;
    if (!runtime.start(options.null_backend, options.device_id, audio_message)) {
      std::fprintf(stderr, "%s\n", audio_message.c_str());
      return 1;
    }
    if (!audio_message.empty()) std::fprintf(stderr, "warning: %s\n", audio_message.c_str());

    molcontrol::ServiceBackend backend(runtime, options.state_directory);
    molcontrol::JsonRpcDispatcher dispatcher;
    if (!molcontrol::register_service_methods(dispatcher, backend)) {
      std::fprintf(stderr, "Could not register the service RPC methods\n");
      runtime.stop();
      return 1;
    }
    std::mutex dispatcher_mutex;
    const auto dispatch = [&](const std::string& request) {
      std::lock_guard<std::mutex> lock(dispatcher_mutex);
      std::string response = dispatcher.dispatch(request);
      if (runtime.shutdown_requested()) stop_requested.store(true, std::memory_order_release);
      return response;
    };

    molcontrol::LoopbackWebSocketServer websocket;
    std::string websocket_token;
    if (options.websocket_enabled) {
      websocket_token = molcontrol::generate_websocket_token();
      molcontrol::LoopbackWebSocketOptions websocket_options;
      websocket_options.port = options.websocket_port;
      websocket_options.token = websocket_token;
      websocket_options.allowed_origins = options.web_origins;
      std::string websocket_error;
      if (!websocket.start(
              websocket_options, dispatch, [&runtime] { return poll_event_notification(runtime); },
              stop_requested, websocket_error)) {
        std::fprintf(stderr, "Could not start WebSocket control: %s\n", websocket_error.c_str());
        runtime.stop();
        return 1;
      }
    }
    const molcontrol::AudioStatus audio = runtime.audio_status();
    std::printf("ready endpoint=%s backend=%s device=%s sample_rate=%u\n", options.endpoint.c_str(),
                audio.backend.c_str(), audio.device_name.c_str(),
                static_cast<unsigned int>(audio.sample_rate));
    std::fflush(stdout);
    if (options.websocket_enabled) {
      std::printf("websocket=ws://127.0.0.1:%u/control token=%s\n",
                  static_cast<unsigned int>(websocket.port()), websocket_token.c_str());
      std::fflush(stdout);
    }

    molcontrol::LocalIpcServer server;
    std::string ipc_error;
    const bool served = server.serve(options.endpoint, dispatch, stop_requested, ipc_error);
    websocket.stop();
    runtime.stop();
    if (!served) {
      std::fprintf(stderr, "Local IPC stopped with an error: %s\n", ipc_error.c_str());
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Service startup failed: %s\n", error.what());
    return 1;
  }
}
