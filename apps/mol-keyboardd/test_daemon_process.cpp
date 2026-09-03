// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#endif

#include "json.hpp"
#include "local_ipc.hpp"

namespace {

#if defined(_WIN32)
std::string quote_argument(const std::string& argument) {
  if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string::npos)
    return argument;
  std::string result = "\"";
  std::size_t backslashes = 0u;
  for (const char character : argument) {
    if (character == '\\') {
      ++backslashes;
    } else if (character == '"') {
      result.append(backslashes * 2u + 1u, '\\');
      result.push_back('"');
      backslashes = 0u;
    } else {
      result.append(backslashes, '\\');
      backslashes = 0u;
      result.push_back(character);
    }
  }
  result.append(backslashes * 2u, '\\');
  result.push_back('"');
  return result;
}
#endif

class ChildProcess {
 public:
  ~ChildProcess() { terminate(); }

  bool start(const std::vector<std::string>& arguments, std::string& error) {
    if (arguments.empty()) return false;
#if defined(_WIN32)
    std::string command_line;
    for (const std::string& argument : arguments) {
      if (!command_line.empty()) command_line.push_back(' ');
      command_line += quote_argument(argument);
    }
    std::vector<char> command(command_line.begin(), command_line.end());
    command.push_back('\0');
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessA(arguments[0].c_str(), command.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) == FALSE) {
      error = "CreateProcess failed with error " +
              std::to_string(static_cast<unsigned long>(GetLastError()));
      return false;
    }
    CloseHandle(process.hThread);
    process_ = process.hProcess;
#else
    const pid_t child = ::fork();
    if (child < 0) {
      error = "fork failed";
      return false;
    }
    if (child == 0) {
      std::vector<char*> values;
      values.reserve(arguments.size() + 1u);
      for (const std::string& argument : arguments)
        values.push_back(const_cast<char*>(argument.c_str()));
      values.push_back(nullptr);
      ::execv(values[0], values.data());
      _exit(127);
    }
    process_ = child;
#endif
    return true;
  }

  bool wait(std::chrono::milliseconds timeout, int& exit_code) {
#if defined(_WIN32)
    if (process_ == nullptr) return false;
    const DWORD result = WaitForSingleObject(process_, static_cast<DWORD>(timeout.count()));
    if (result != WAIT_OBJECT_0) return false;
    DWORD code = 1u;
    if (GetExitCodeProcess(process_, &code) == FALSE) return false;
    exit_code = static_cast<int>(code);
    CloseHandle(process_);
    process_ = nullptr;
    return true;
#else
    if (process_ <= 0) return false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      int status = 0;
      const pid_t result = ::waitpid(process_, &status, WNOHANG);
      if (result == process_) {
        process_ = -1;
        exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        return true;
      }
      if (result < 0) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
#endif
  }

  void terminate() {
#if defined(_WIN32)
    if (process_ != nullptr) {
      (void)TerminateProcess(process_, 1u);
      (void)WaitForSingleObject(process_, 2000u);
      CloseHandle(process_);
      process_ = nullptr;
    }
#else
    if (process_ > 0) {
      (void)::kill(process_, SIGTERM);
      int ignored = 0;
      (void)::waitpid(process_, &ignored, 0);
      process_ = -1;
    }
#endif
  }

 private:
#if defined(_WIN32)
  HANDLE process_ = nullptr;
#else
  pid_t process_ = -1;
#endif
};

bool rpc(const std::string& endpoint, const std::string& method, const std::string& params,
         molseq::Json& result, std::string& error) {
  static std::uint64_t id = 1u;
  const std::string request = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method +
                              "\",\"params\":" + params + ",\"id\":" + std::to_string(id++) + "}";
  std::string response_text;
  if (!molcontrol::send_local_ipc_request(endpoint, request, response_text, error)) return false;
  try {
    const molseq::Json response = molseq::parse_json(response_text);
    if (molseq::optional_member(response, "error") != nullptr) {
      error = "RPC error from " + method + ": " + response_text;
      return false;
    }
    result = molseq::require_member(response, "result");
    return true;
  } catch (const std::exception& parse_error) {
    error = std::string("invalid RPC response: ") + parse_error.what();
    return false;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  const std::filesystem::path daemon = std::filesystem::absolute(argv[1]);
  const std::filesystem::path state = std::filesystem::absolute(argv[2]);
  const std::string endpoint = argv[3];
  std::error_code filesystem_error;
  std::filesystem::remove_all(state, filesystem_error);
  filesystem_error.clear();
  std::filesystem::create_directories(state, filesystem_error);
  if (filesystem_error) return 1;
  struct Cleanup {
    std::filesystem::path state;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(state, ignored);
    }
  } cleanup{state};

  ChildProcess process;
  std::string error;
  std::vector<std::string> daemon_command;
#if defined(__linux__)
  const char* test_emulator = std::getenv("MOL_TEST_EXECUTABLE_EMULATOR");
  if (test_emulator != nullptr && test_emulator[0] != '\0')
    daemon_command.emplace_back(test_emulator);
#endif
  daemon_command.insert(daemon_command.end(),
                        {daemon.string(), "--null-backend", "--state-dir", state.string(),
                         "--endpoint", endpoint});
  if (!process.start(daemon_command, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  molseq::Json result;
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    ready = rpc(endpoint, "engine.getState", "{}", result, error);
    if (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  const auto call = [&](const std::string& method, const std::string& params = "{}") {
    if (rpc(endpoint, method, params, result, error)) return true;
    std::fprintf(stderr, "%s\n", error.c_str());
    return false;
  };
  if (!ready || !call("preset.select", "{\"preset\":\"violin\"}") ||
      !call("preset.setParameter", "{\"parameter\":11,\"value\":0.2}") ||
      !call("recording.start") ||
      !call("performance.noteOn", "{\"note\":60,\"velocity\":0.8,\"gesture\":7001}")) {
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  if (!call("performance.noteOff", "{\"gesture\":7001}") ||
      !call("recording.stop", "{\"name\":\"process-test.molseq\"}") ||
      !call("recording.load", "{\"name\":\"process-test.molseq\"}") || !call("playback.start") ||
      !call("playback.stop") || !call("diagnostics.selfTest") || !call("diagnostics.doctor") ||
      !call("diagnostics.benchmark", "{\"frames\":4096}") || !call("engine.allSoundOff") ||
      !call("system.shutdown")) {
    return 1;
  }
  int exit_code = 1;
  const bool exited = process.wait(std::chrono::seconds(5), exit_code);
  const bool recording_exists =
      std::filesystem::is_regular_file(state / "recordings" / "process-test.molseq");
  const bool config_exists = std::filesystem::is_regular_file(state / "config.json");
  if (!exited || exit_code != 0 || !recording_exists || !config_exists) {
    std::fprintf(stderr, "daemon validation failed: exited=%d code=%d recording=%d config=%d\n",
                 exited ? 1 : 0, exit_code, recording_exists ? 1 : 0, config_exists ? 1 : 0);
    std::error_code list_error;
    for (std::filesystem::directory_iterator iterator(state, list_error), end;
         !list_error && iterator != end; iterator.increment(list_error))
      std::fprintf(stderr, "state entry: %s\n", iterator->path().string().c_str());
    return 1;
  }
  return 0;
}
