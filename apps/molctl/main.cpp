// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "json.hpp"
#include "local_ipc.hpp"
#include "service_paths.hpp"

namespace {

using Json = molseq::Json;

struct Options {
  bool json = false;
  std::filesystem::path state_directory = molcontrol::default_service_state_directory();
  std::string endpoint;
  std::vector<std::string> arguments;
};

struct Invocation {
  std::string method;
  Json params = Json::object_value({});
  bool render = false;
  std::vector<std::string> render_arguments;
  std::filesystem::path copy_recording_to;
  std::filesystem::path staged_playback;
  std::filesystem::path service_recording;
};

void print_usage(const char* executable) {
  std::printf(
      "Usage: %s [--json] [--endpoint LOCAL_ENDPOINT] [--state-dir PATH] COMMAND\n"
      "\n"
      "Commands:\n"
      "  status | capabilities | devices input|output\n"
      "  input attach ID | output select ID\n"
      "  preset list | preset set NAME\n"
      "  note on NOTE [--velocity 0..1] [--gesture ID]\n"
      "  note off --gesture ID | sustain on|off | tempo BPM\n"
      "  chord MODE | arpeggiator MODE [--rate 1/16] [--gate 0..1] [--octaves 1..4]\n"
      "  record start | record stop [--output FILE.molseq]\n"
      "  play FILE.molseq | render FILE.molseq --output FILE.wav\n"
      "  all-notes-off | doctor | self-test | benchmark | shutdown\n"
      "  rpc METHOD [JSON_PARAMS]\n",
      executable);
}

bool parse_global_options(int argc, char** argv, Options& options) {
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--json") {
      options.json = true;
    } else if (argument == "--endpoint" && index + 1 < argc) {
      options.endpoint = argv[++index];
    } else if (argument == "--state-dir" && index + 1 < argc) {
      options.state_directory = argv[++index];
    } else {
      for (; index < argc; ++index) options.arguments.emplace_back(argv[index]);
      break;
    }
  }
  if (options.endpoint.empty())
    options.endpoint = molcontrol::default_local_ipc_endpoint(options.state_directory);
  return !options.arguments.empty();
}

bool parse_u64(const std::string& text, std::uint64_t minimum, std::uint64_t maximum,
               std::uint64_t& value) {
  if (text.empty()) return false;
  for (const char character : text)
    if (character < '0' || character > '9') return false;
  char* end = nullptr;
  errno = 0;
  const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || end == nullptr || *end != '\0' || parsed < minimum ||
      parsed > maximum)
    return false;
  value = static_cast<std::uint64_t>(parsed);
  return true;
}

bool parse_real(const std::string& text, double minimum, double maximum, double& value) {
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || end == text.c_str() || end == nullptr || *end != '\0' ||
      !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
    return false;
  value = parsed;
  return true;
}

Json object(std::initializer_list<std::pair<const std::string, Json>> members) {
  Json::Object result;
  for (const auto& member : members) result.emplace(member.first, member.second);
  return Json::object_value(std::move(result));
}

bool flag_value(const std::vector<std::string>& arguments, const std::string& flag,
                std::string& value) {
  for (std::size_t index = 0u; index < arguments.size(); ++index) {
    if (arguments[index] == flag && index + 1u < arguments.size()) {
      value = arguments[index + 1u];
      return true;
    }
  }
  return false;
}

bool validate_flags(const std::vector<std::string>& arguments, std::size_t first,
                    std::initializer_list<std::string_view> allowed, std::string& error) {
  if ((arguments.size() - first) % 2u != 0u) {
    error = "every option requires a value";
    return false;
  }
  std::vector<std::string_view> seen;
  for (std::size_t index = first; index < arguments.size(); index += 2u) {
    const std::string_view flag = arguments[index];
    if (std::find(allowed.begin(), allowed.end(), flag) == allowed.end()) {
      error = "unknown option: " + arguments[index];
      return false;
    }
    if (std::find(seen.begin(), seen.end(), flag) != seen.end()) {
      error = "duplicate option: " + arguments[index];
      return false;
    }
    seen.push_back(flag);
  }
  return true;
}

int named_value(const std::string& name, const std::vector<std::string_view>& names) {
  for (std::size_t index = 0u; index < names.size(); ++index)
    if (name == names[index]) return static_cast<int>(index);
  return -1;
}

bool build_invocation(const Options& options, Invocation& invocation, std::string& error) {
  const std::vector<std::string>& args = options.arguments;
  const std::string& command = args[0];
  if (command == "status" && args.size() == 1u) {
    invocation.method = "engine.getState";
  } else if (command == "capabilities" && args.size() == 1u) {
    invocation.method = "system.getCapabilities";
  } else if (command == "devices" && args.size() == 2u && args[1] == "input") {
    invocation.method = "input.listDevices";
  } else if (command == "devices" && args.size() == 2u && args[1] == "output") {
    invocation.method = "audio.listDevices";
  } else if (command == "input" && args.size() == 3u && args[1] == "attach") {
    invocation.method = "input.attach";
    invocation.params = object({{"id", Json::string(args[2])}});
  } else if (command == "output" && args.size() == 3u && args[1] == "select") {
    invocation.method = "audio.selectDevice";
    invocation.params = object({{"id", Json::string(args[2])}});
  } else if (command == "preset" && args.size() == 2u && args[1] == "list") {
    invocation.method = "preset.list";
  } else if (command == "preset" && args.size() == 3u && args[1] == "set") {
    invocation.method = "preset.select";
    invocation.params = object({{"preset", Json::string(args[2])}});
  } else if (command == "note" && args.size() >= 3u && args[1] == "on") {
    if (!validate_flags(args, 3u, {"--velocity", "--gesture"}, error)) return false;
    std::uint64_t note = 0u;
    if (!parse_u64(args[2], 0u, 127u, note)) {
      error = "note must be a MIDI note from 0 through 127";
      return false;
    }
    double velocity = 0.8;
    std::string value;
    if (flag_value(args, "--velocity", value) && !parse_real(value, 0.000001, 1.0, velocity)) {
      error = "velocity must be greater than zero and at most one";
      return false;
    }
    std::uint64_t gesture =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    if (gesture == 0u) gesture = 1u;
    if (flag_value(args, "--gesture", value) &&
        !parse_u64(value, 1u, std::numeric_limits<std::uint64_t>::max(), gesture)) {
      error = "gesture must be a nonzero unsigned integer";
      return false;
    }
    invocation.method = "performance.noteOn";
    invocation.params = object({{"gesture", Json::number(gesture)},
                                {"note", Json::number(note)},
                                {"velocity", Json::number(velocity)}});
  } else if (command == "note" && args.size() >= 2u && args[1] == "off") {
    if (!validate_flags(args, 2u, {"--gesture"}, error)) return false;
    std::string value;
    std::uint64_t gesture = 0u;
    if (!flag_value(args, "--gesture", value) ||
        !parse_u64(value, 1u, std::numeric_limits<std::uint64_t>::max(), gesture)) {
      error = "note off requires --gesture with a nonzero unsigned integer";
      return false;
    }
    invocation.method = "performance.noteOff";
    invocation.params = object({{"gesture", Json::number(gesture)}});
  } else if (command == "sustain" && args.size() == 2u && (args[1] == "on" || args[1] == "off")) {
    invocation.method = "performance.control";
    invocation.params = object(
        {{"control", Json::string("sustain")}, {"value", Json::number(args[1] == "on" ? 1 : 0)}});
  } else if (command == "tempo" && args.size() == 2u) {
    double tempo = 0.0;
    if (!parse_real(args[1], 30.0, 300.0, tempo)) {
      error = "tempo must be from 30 through 300 BPM";
      return false;
    }
    invocation.method = "transport.setTempo";
    invocation.params = object({{"bpm", Json::number(tempo)}});
  } else if (command == "chord" && args.size() == 2u) {
    const int mode = named_value(args[1], {"off", "major", "minor", "sus2", "sus4", "dominant-7",
                                           "major-7", "minor-7", "power-5", "octave"});
    if (mode < 0) {
      error = "unknown chord mode";
      return false;
    }
    invocation.method = "performance.control";
    invocation.params = object({{"control", Json::string("chord")}, {"value", Json::number(mode)}});
  } else if (command == "arpeggiator" && args.size() >= 2u) {
    if (!validate_flags(args, 2u, {"--rate", "--gate", "--octaves"}, error)) return false;
    const int mode =
        named_value(args[1], {"off", "up", "down", "up-down", "down-up", "as-played", "random"});
    if (mode < 0) {
      error = "unknown arpeggiator mode";
      return false;
    }
    std::string rate_text = "1/16";
    (void)flag_value(args, "--rate", rate_text);
    const int rate = named_value(rate_text, {"1/4", "1/8", "1/8t", "1/16", "1/16t", "1/32"});
    if (rate < 0) {
      error = "arpeggiator rate must be 1/4, 1/8, 1/8t, 1/16, 1/16t, or 1/32";
      return false;
    }
    double gate = 0.5;
    std::string value;
    if (flag_value(args, "--gate", value) && !parse_real(value, 0.05, 1.0, gate)) {
      error = "arpeggiator gate must be from 0.05 through 1";
      return false;
    }
    std::uint64_t octaves = 1u;
    if (flag_value(args, "--octaves", value) && !parse_u64(value, 1u, 4u, octaves)) {
      error = "arpeggiator octaves must be from 1 through 4";
      return false;
    }
    invocation.method = "performance.control";
    invocation.params = object({{"control", Json::string("arpeggiator")},
                                {"gate", Json::number(gate)},
                                {"mode", Json::number(mode)},
                                {"octaves", Json::number(octaves)},
                                {"rate", Json::number(rate)}});
  } else if (command == "record" && args.size() == 2u && args[1] == "start") {
    invocation.method = "recording.start";
  } else if (command == "record" && args.size() >= 2u && args[1] == "stop") {
    if (!validate_flags(args, 2u, {"--output"}, error)) return false;
    invocation.method = "recording.stop";
    std::string output;
    if (flag_value(args, "--output", output)) {
      invocation.copy_recording_to = std::filesystem::absolute(output).lexically_normal();
      const std::string name =
          "molctl-recording-" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".molseq";
      invocation.service_recording = options.state_directory / "recordings" / name;
      invocation.params = object({{"name", Json::string(name)}});
    }
  } else if (command == "play" && args.size() == 2u) {
    const std::filesystem::path input = std::filesystem::absolute(args[1]).lexically_normal();
    if (!std::filesystem::is_regular_file(input)) {
      error = "playback input is not a regular file: " + input.string();
      return false;
    }
    const std::string name =
        "molctl-playback-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".molseq";
    invocation.staged_playback = options.state_directory / "recordings" / name;
    std::error_code filesystem_error;
    std::filesystem::create_directories(invocation.staged_playback.parent_path(), filesystem_error);
    if (!filesystem_error)
      std::filesystem::copy_file(input, invocation.staged_playback,
                                 std::filesystem::copy_options::overwrite_existing,
                                 filesystem_error);
    if (filesystem_error) {
      error = "cannot stage playback input: " + filesystem_error.message();
      return false;
    }
    invocation.method = "playback.start";
    invocation.params = object({{"name", Json::string(name)}});
  } else if (command == "render" && args.size() == 4u && args[2] == "--output") {
    invocation.render = true;
    invocation.render_arguments = {args[1], "--output", args[3]};
  } else if (command == "all-notes-off" && args.size() == 1u) {
    invocation.method = "engine.allNotesOff";
  } else if (command == "doctor" && args.size() == 1u) {
    invocation.method = "diagnostics.doctor";
  } else if (command == "self-test" && args.size() == 1u) {
    invocation.method = "diagnostics.selfTest";
  } else if (command == "benchmark" && args.size() == 1u) {
    invocation.method = "diagnostics.benchmark";
  } else if (command == "shutdown" && args.size() == 1u) {
    invocation.method = "system.shutdown";
  } else if (command == "rpc" && (args.size() == 2u || args.size() == 3u)) {
    invocation.method = args[1];
    if (args.size() == 3u) {
      try {
        invocation.params = molseq::parse_json(args[2]);
      } catch (const std::exception& parse_error) {
        error = std::string("invalid JSON parameters: ") + parse_error.what();
        return false;
      }
    }
  } else {
    error = "unknown command or invalid arguments";
    return false;
  }
  return true;
}

std::filesystem::path executable_directory(const char* argument_zero) {
#if defined(_WIN32)
  std::vector<char> buffer(32768u, '\0');
  const DWORD length =
      GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length != 0u && length < buffer.size())
    return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
#elif defined(__linux__)
  std::array<char, 4096u> buffer{};
  const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1u);
  if (length > 0)
    return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length)))
        .parent_path();
#endif
  return std::filesystem::absolute(argument_zero).parent_path();
}

#if defined(_WIN32)
std::string quote_windows_argument(const std::string& argument) {
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

int run_program(const std::vector<std::string>& arguments, std::string& error) {
  if (arguments.empty()) return 2;
#if defined(_WIN32)
  std::string command_line;
  for (const std::string& argument : arguments) {
    if (!command_line.empty()) command_line.push_back(' ');
    command_line += quote_windows_argument(argument);
  }
  std::vector<char> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back('\0');
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (CreateProcessA(arguments[0].c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0u,
                     nullptr, nullptr, &startup, &process) == FALSE) {
    error = "cannot start renderer; Windows error " +
            std::to_string(static_cast<unsigned long>(GetLastError()));
    return 1;
  }
  CloseHandle(process.hThread);
  const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1u;
  if (wait_result != WAIT_OBJECT_0 || GetExitCodeProcess(process.hProcess, &exit_code) == FALSE)
    error = "cannot obtain renderer exit status";
  CloseHandle(process.hProcess);
  return static_cast<int>(exit_code);
#else
  const pid_t child = ::fork();
  if (child < 0) {
    error = "cannot fork renderer";
    return 1;
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
  int status = 0;
  if (::waitpid(child, &status, 0) < 0) {
    error = "cannot wait for renderer";
    return 1;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  error = "renderer terminated abnormally";
  return 1;
#endif
}

void print_indent(int depth) {
  for (int index = 0; index < depth; ++index) std::fputs("  ", stdout);
}

void print_scalar(const Json& value) {
  switch (value.type) {
    case Json::Type::Null:
      std::fputs("null", stdout);
      break;
    case Json::Type::Boolean:
      std::fputs(value.boolean ? "yes" : "no", stdout);
      break;
    case Json::Type::Number:
    case Json::Type::String:
      std::fputs(value.text.c_str(), stdout);
      break;
    default:
      break;
  }
}

bool scalar(const Json& value) {
  return value.type != Json::Type::Array && value.type != Json::Type::Object;
}

void print_human(const Json& value, int depth = 0) {
  if (value.type == Json::Type::Object) {
    for (const auto& member : value.object) {
      print_indent(depth);
      std::printf("%s:", member.first.c_str());
      if (scalar(member.second)) {
        std::fputc(' ', stdout);
        print_scalar(member.second);
        std::fputc('\n', stdout);
      } else {
        std::fputc('\n', stdout);
        print_human(member.second, depth + 1);
      }
    }
  } else if (value.type == Json::Type::Array) {
    for (const Json& item : value.array) {
      print_indent(depth);
      std::fputs("-", stdout);
      if (scalar(item)) {
        std::fputc(' ', stdout);
        print_scalar(item);
        std::fputc('\n', stdout);
      } else {
        std::fputc('\n', stdout);
        print_human(item, depth + 1);
      }
    }
  } else {
    print_indent(depth);
    print_scalar(value);
    std::fputc('\n', stdout);
  }
}

int execute_rpc(const Options& options, Invocation& invocation) {
  Json::Object request;
  request["id"] = Json::number(1);
  request["jsonrpc"] = Json::string("2.0");
  request["method"] = Json::string(invocation.method);
  request["params"] = invocation.params;
  std::string response_text;
  std::string error;
  if (!molcontrol::send_local_ipc_request(
          options.endpoint, molseq::write_json(Json::object_value(std::move(request))),
          response_text, error)) {
    std::fprintf(stderr, "Cannot contact mol-keyboardd at %s: %s\n", options.endpoint.c_str(),
                 error.c_str());
    return 1;
  }
  try {
    const Json response = molseq::parse_json(response_text);
    const Json* rpc_error = molseq::optional_member(response, "error");
    if (rpc_error != nullptr) {
      if (options.json)
        std::printf("%s\n", response_text.c_str());
      else
        std::fprintf(stderr, "Service error: %s\n", molseq::write_json(*rpc_error).c_str());
      return 1;
    }
    if (!invocation.copy_recording_to.empty()) {
      std::error_code copy_error;
      std::filesystem::copy_file(invocation.service_recording, invocation.copy_recording_to,
                                 std::filesystem::copy_options::overwrite_existing, copy_error);
      if (copy_error) {
        std::fprintf(stderr, "Cannot copy recording to %s: %s\n",
                     invocation.copy_recording_to.string().c_str(), copy_error.message().c_str());
        return 1;
      }
    }
    if (options.json)
      std::printf("%s\n", response_text.c_str());
    else
      print_human(molseq::require_member(response, "result"));
    return 0;
  } catch (const std::exception& parse_error) {
    std::fprintf(stderr, "Service returned invalid JSON: %s\n", parse_error.what());
    return 1;
  }
}

}  // namespace

int main(int argc, char** argv) try {
  Options options;
  if (!parse_global_options(argc, argv, options) ||
      (options.arguments.size() == 1u && options.arguments[0] == "--help")) {
    print_usage(argv[0]);
    return options.arguments.empty() ? 2 : 0;
  }
  Invocation invocation;
  std::string error;
  if (!build_invocation(options, invocation, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    print_usage(argv[0]);
    return 2;
  }
  if (invocation.render) {
#if defined(_WIN32)
    const char* renderer_name = "mol-render.exe";
#else
    const char* renderer_name = "mol-render";
#endif
    const std::filesystem::path directory = executable_directory(argv[0]);
    std::filesystem::path renderer = directory / renderer_name;
    if (!std::filesystem::is_regular_file(renderer))
      renderer = directory.parent_path() / "mol-render" / renderer_name;
    std::vector<std::string> arguments;
#if defined(__linux__)
    const char* test_emulator = std::getenv("MOL_TEST_EXECUTABLE_EMULATOR");
    if (test_emulator != nullptr && test_emulator[0] != '\0')
      arguments.emplace_back(test_emulator);
#endif
    arguments.emplace_back(renderer.string());
    arguments.insert(arguments.end(), invocation.render_arguments.begin(),
                     invocation.render_arguments.end());
    const int result = run_program(arguments, error);
    if (result != 0 && !error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
    return result;
  }
  const int result = execute_rpc(options, invocation);
  std::error_code ignored;
  if (!invocation.staged_playback.empty())
    std::filesystem::remove(invocation.staged_playback, ignored);
  if (!invocation.service_recording.empty())
    std::filesystem::remove(invocation.service_recording, ignored);
  return result;
} catch (const std::exception& error) {
  std::fprintf(stderr, "molctl failed: %s\n", error.what());
  return 1;
}
