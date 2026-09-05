// SPDX-License-Identifier: Apache-2.0
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "desktop_rpc_client.hpp"
#include "local_ipc.hpp"
#include "service_paths.hpp"

namespace {

using Json = molseq::Json;

int g_exit_code = 0;

std::filesystem::path NativePath(const wxString& value) {
#if defined(_WIN32)
  return std::filesystem::path(value.ToStdWstring());
#else
  return std::filesystem::path(value.ToStdString());
#endif
}

std::string Utf8(const wxString& value) {
  const wxScopedCharBuffer bytes = value.ToUTF8();
  return bytes ? std::string(bytes.data()) : std::string();
}

wxString FromUtf8(const std::string& value) { return wxString::FromUTF8(value); }

Json Object(std::initializer_list<std::pair<const std::string, Json>> members) {
  Json::Object object;
  for (const auto& member : members) object.emplace(member.first, member.second);
  return Json::object_value(std::move(object));
}

struct Options {
  std::string endpoint;
  std::filesystem::path acceptance_output;
};

bool ParseOptions(int count, wxChar** values, Options& options, wxString& error) {
  std::filesystem::path state_directory = molcontrol::default_service_state_directory();
  for (int index = 1; index < count; ++index) {
    const wxString argument(values[index]);
    if (argument == "--endpoint" || argument == "--state-dir" ||
        argument == "--acceptance-output") {
      if (++index >= count) {
        error = argument + " requires a value.";
        return false;
      }
      if (argument == "--endpoint")
        options.endpoint = Utf8(values[index]);
      else if (argument == "--state-dir")
        state_directory = NativePath(values[index]);
      else
        options.acceptance_output = NativePath(values[index]);
    } else {
      error = "Unknown option: " + argument;
      return false;
    }
  }
  if (options.endpoint.empty())
    options.endpoint = molcontrol::default_local_ipc_endpoint(state_directory);
  return true;
}

class DebugFrame final : public wxFrame {
 public:
  explicit DebugFrame(Options options)
      : wxFrame(nullptr, wxID_ANY, "MoL Keyboard - Native Service Debugger", wxDefaultPosition,
                wxSize(1040, 760)),
        acceptance_output_(std::move(options.acceptance_output)),
        client_(std::move(options.endpoint)),
        acceptance_timer_(this) {
    BuildInterface();
    Bind(wxEVT_CLOSE_WINDOW, &DebugFrame::OnClose, this);
    Bind(wxEVT_TIMER, &DebugFrame::OnAcceptanceTimer, this, acceptance_timer_.GetId());
    SetMinSize(wxSize(820, 620));
    Centre();
    if (!acceptance_output_.empty()) acceptance_timer_.Start(20);
  }

 private:
  static constexpr int kFirstNote = 60;
  static constexpr int kNoteCount = 18;

  void BuildInterface() {
    auto* panel = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* connection = new wxBoxSizer(wxHORIZONTAL);
    connection->Add(new wxStaticText(panel, wxID_ANY, "Local IPC endpoint"), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    endpoint_ = new wxTextCtrl(panel, wxID_ANY, FromUtf8(client_.endpoint()));
    connection->Add(endpoint_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    auto* refresh = new wxButton(panel, wxID_ANY, "Connect / Refresh");
    connection->Add(refresh, 0, wxRIGHT, 10);
    connection_state_ = new wxStaticText(panel, wxID_ANY, "Not connected");
    connection->Add(connection_state_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(connection, 0, wxEXPAND | wxALL, 10);
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshService(); });

    auto* performance = new wxStaticBoxSizer(wxVERTICAL, panel, "Performance controls");
    auto* controls = new wxBoxSizer(wxHORIZONTAL);
    controls->Add(new wxStaticText(panel, wxID_ANY, "Preset"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    preset_ = new wxChoice(panel, wxID_ANY);
    static constexpr const char* kPresets[] = {
        "grand-piano",   "electric-piano", "harpsichord", "music-box",    "vibraphone",
        "church-organ",  "jazz-organ",     "nylon-guitar", "steel-guitar", "violin",
        "cello",         "harp",           "choir",        "flute",        "clarinet",
        "synth-lead",    "synth-pad",      "synth-bass"};
    for (const char* preset : kPresets) preset_->Append(preset);
    preset_->SetSelection(0);
    controls->Add(preset_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
    controls->Add(new wxStaticText(panel, wxID_ANY, "Tempo"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    tempo_ = new wxSpinCtrlDouble(panel, wxID_ANY, "120", wxDefaultPosition, wxSize(84, -1),
                                  wxSP_ARROW_KEYS, 30.0, 300.0, 120.0, 1.0);
    tempo_->SetDigits(0);
    controls->Add(tempo_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    auto* apply_tempo = new wxButton(panel, wxID_ANY, "Apply");
    controls->Add(apply_tempo, 0, wxRIGHT, 14);
    controls->Add(new wxStaticText(panel, wxID_ANY, "Velocity"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    velocity_ = new wxSlider(panel, wxID_ANY, 80, 1, 100, wxDefaultPosition, wxSize(150, -1),
                             wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
    controls->Add(velocity_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
    sustain_ = new wxCheckBox(panel, wxID_ANY, "Sustain");
    controls->Add(sustain_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
    auto* silence = new wxButton(panel, wxID_ANY, "All sound off");
    controls->Add(silence, 0);
    performance->Add(controls, 0, wxEXPAND | wxALL, 8);

    auto* keyboard = new wxGridSizer(2, 9, 4, 4);
    static constexpr const char* kNoteNames[kNoteCount] = {
        "C4",  "C#4", "D4",  "D#4", "E4",  "F4",  "F#4", "G4", "G#4",
        "A4",  "A#4", "B4",  "C5",  "C#5", "D5",  "D#5", "E5", "F5"};
    for (int offset = 0; offset < kNoteCount; ++offset) {
      const int note = kFirstNote + offset;
      auto* key = new wxButton(panel, wxID_ANY, kNoteNames[offset], wxDefaultPosition,
                               wxSize(68, 54));
      if (std::string(kNoteNames[offset]).find('#') != std::string::npos) {
        key->SetBackgroundColour(wxColour(52, 58, 64));
        key->SetForegroundColour(*wxWHITE);
      }
      key->Bind(wxEVT_LEFT_DOWN, [this, key, note](wxMouseEvent& event) {
        PressNote(note);
        if (!key->HasCapture()) key->CaptureMouse();
        event.Skip();
      });
      key->Bind(wxEVT_LEFT_UP, [this, key, note](wxMouseEvent& event) {
        ReleaseNote(note);
        if (key->HasCapture()) key->ReleaseMouse();
        event.Skip();
      });
      key->Bind(wxEVT_MOUSE_CAPTURE_LOST,
                [this, note](wxMouseCaptureLostEvent&) { ReleaseNote(note); });
      keyboard->Add(key, 1, wxEXPAND);
    }
    performance->Add(keyboard, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    root->Add(performance, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    preset_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
      Call("preset.select",
           Object({{"preset", Json::string(Utf8(preset_->GetStringSelection()))}}));
    });
    apply_tempo->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
      Call("transport.setTempo", Object({{"bpm", Json::number(tempo_->GetValue())}}));
    });
    sustain_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
      Call("performance.control",
           Object({{"control", Json::string("sustain")},
                   {"value", Json::number(sustain_->GetValue() ? 1 : 0)}}));
    });
    silence->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AllSoundOff(); });

    auto* operations = new wxStaticBoxSizer(wxHORIZONTAL, panel, "Service operations");
    AddOperationButton(panel, operations, "State", "engine.getState");
    AddOperationButton(panel, operations, "Capabilities", "system.getCapabilities");
    AddOperationButton(panel, operations, "Audio devices", "audio.listDevices");
    AddOperationButton(panel, operations, "Start recording", "recording.start");
    AddOperationButton(panel, operations, "Stop recording", "recording.stop");
    AddOperationButton(panel, operations, "Self-test", "diagnostics.selfTest");
    AddOperationButton(panel, operations, "Doctor", "diagnostics.doctor");
    auto* benchmark = new wxButton(panel, wxID_ANY, "Benchmark");
    benchmark->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
      Call("diagnostics.benchmark", Object({{"frames", Json::number(96000)}}));
    });
    operations->Add(benchmark, 0, wxALL, 4);
    root->Add(operations, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    log_ = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                          wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
    log_->SetFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));
    root->Add(log_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    panel->SetSizer(root);
  }

  void AddOperationButton(wxWindow* parent, wxSizer* target, const wxString& label,
                          std::string method) {
    auto* button = new wxButton(parent, wxID_ANY, label);
    button->Bind(wxEVT_BUTTON,
                 [this, method = std::move(method)](wxCommandEvent&) { Call(method); });
    target->Add(button, 0, wxALL, 4);
  }

  moldesktop::RpcResult Call(const std::string& method,
                             const Json& params = Json::object_value({}), bool log = true) {
    client_.set_endpoint(Utf8(endpoint_->GetValue()));
    moldesktop::RpcResult result = client_.Call(method, params);
    connection_state_->SetLabel(result.ok ? "Connected" : "Unavailable");
    connection_state_->SetForegroundColour(result.ok ? wxColour(0, 120, 40)
                                                       : wxColour(180, 35, 35));
    if (log) {
      log_->AppendText(FromUtf8("> " + method + "\n"));
      log_->AppendText(FromUtf8((result.ok ? result.response : result.error) + "\n\n"));
      log_->ShowPosition(log_->GetLastPosition());
    }
    return result;
  }

  void RefreshService() {
    log_->Clear();
    const moldesktop::RpcResult state = Call("engine.getState");
    if (state.ok) (void)Call("system.getCapabilities");
  }

  void PressNote(int note) {
    if (active_gestures_.find(note) != active_gestures_.end()) return;
    const std::uint64_t gesture = next_gesture_++;
    const double velocity = static_cast<double>(velocity_->GetValue()) / 100.0;
    const moldesktop::RpcResult result =
        Call("performance.noteOn", Object({{"gesture", Json::number(gesture)},
                                           {"note", Json::number(note)},
                                           {"velocity", Json::number(velocity)}}),
             false);
    if (result.ok) active_gestures_[note] = gesture;
  }

  void ReleaseNote(int note) {
    const auto found = active_gestures_.find(note);
    if (found == active_gestures_.end()) return;
    const std::uint64_t gesture = found->second;
    active_gestures_.erase(found);
    (void)Call("performance.noteOff", Object({{"gesture", Json::number(gesture)}}), false);
  }

  void AllSoundOff() {
    active_gestures_.clear();
    (void)Call("engine.allSoundOff");
  }

  void OnAcceptanceTimer(wxTimerEvent&) {
    if (acceptance_finished_) return;
    ++acceptance_attempts_;
    const moldesktop::RpcResult state = Call("engine.getState", Json::object_value({}), false);
    if (!state.ok && acceptance_attempts_ < 200) return;
    bool passed = state.ok;
    std::string detail = state.ok ? "engine.getState=" + state.response : state.error;
    if (passed) passed = Call("system.getCapabilities", Json::object_value({}), false).ok;
    if (passed) passed = Call("diagnostics.selfTest", Json::object_value({}), false).ok;
    if (passed) {
      passed = Call("performance.noteOn",
                    Object({{"gesture", Json::number(9000001)}, {"note", Json::number(60)},
                            {"velocity", Json::number(0.8)}}),
                    false)
                   .ok;
    }
    if (passed)
      passed = Call("performance.noteOff", Object({{"gesture", Json::number(9000001)}}), false).ok;
    if (passed) passed = Call("engine.allSoundOff", Json::object_value({}), false).ok;
    if (passed) passed = Call("system.shutdown", Json::object_value({}), false).ok;
    FinishAcceptance(passed, detail);
  }

  void FinishAcceptance(bool passed, const std::string& detail) {
    if (acceptance_finished_) return;
    acceptance_finished_ = true;
    acceptance_timer_.Stop();
    std::ofstream report(acceptance_output_, std::ios::binary);
    report << "passed=" << (passed ? "true" : "false") << '\n'
           << "detail=" << detail << '\n'
           << "endpoint=" << client_.endpoint() << '\n';
    if (!report.good()) passed = false;
    g_exit_code = passed ? 0 : 1;
    CallAfter([this]() { Close(true); });
  }

  void OnClose(wxCloseEvent& event) {
    acceptance_timer_.Stop();
    if (acceptance_output_.empty() && !active_gestures_.empty()) AllSoundOff();
    event.Skip();
  }

  std::filesystem::path acceptance_output_;
  moldesktop::RpcClient client_;
  wxTimer acceptance_timer_;
  wxTextCtrl* endpoint_ = nullptr;
  wxStaticText* connection_state_ = nullptr;
  wxChoice* preset_ = nullptr;
  wxSpinCtrlDouble* tempo_ = nullptr;
  wxSlider* velocity_ = nullptr;
  wxCheckBox* sustain_ = nullptr;
  wxTextCtrl* log_ = nullptr;
  std::unordered_map<int, std::uint64_t> active_gestures_;
  std::uint64_t next_gesture_ = 100000u;
  int acceptance_attempts_ = 0;
  bool acceptance_finished_ = false;
};

class DebugApp final : public wxApp {
 public:
  bool OnInit() override {
    SetAppName("MoL Keyboard Debugger");
    SetVendorName("MoL Keyboard contributors");
    Options options;
    wxString error;
    if (!ParseOptions(argc, argv, options, error)) {
      wxMessageBox(error, "MoL Keyboard Debugger", wxOK | wxICON_ERROR);
      g_exit_code = 2;
      return false;
    }
    auto* frame = new DebugFrame(std::move(options));
    frame->Show();
    SetTopWindow(frame);
    return true;
  }

  int OnExit() override { return g_exit_code; }
};

}  // namespace

wxIMPLEMENT_APP(DebugApp);
