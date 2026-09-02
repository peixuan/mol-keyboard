// SPDX-License-Identifier: Apache-2.0
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "audio_runtime.hpp"
#include "local_ipc.hpp"
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
};

void print_usage(const char* executable) {
  std::printf(
      "Usage: %s [--null-backend] [--device-id ID] [--state-dir PATH]\n"
      "          [--endpoint LOCAL_ENDPOINT] [--version] [--help]\n"
      "\n"
      "Runs in the foreground as the current user. Use platform user-service files\n"
      "for background startup. No network listener or Web UI is started.\n",
      executable);
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
    } else {
      std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argument.c_str());
      return false;
    }
  }
  return true;
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
    const molcontrol::AudioStatus audio = runtime.audio_status();
    std::printf("ready endpoint=%s backend=%s device=%s sample_rate=%u\n", options.endpoint.c_str(),
                audio.backend.c_str(), audio.device_name.c_str(),
                static_cast<unsigned int>(audio.sample_rate));
    std::fflush(stdout);

    molcontrol::LocalIpcServer server;
    std::string ipc_error;
    const bool served = server.serve(
        options.endpoint,
        [&](const std::string& request) {
          std::string response = dispatcher.dispatch(request);
          if (runtime.shutdown_requested()) stop_requested.store(true, std::memory_order_release);
          return response;
        },
        stop_requested, ipc_error);
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
