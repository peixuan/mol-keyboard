// SPDX-License-Identifier: Apache-2.0
#include <wx/wx.h>
#include <wx/snglinst.h>
#include <wx/stdpaths.h>
#include <wx/webview.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "desktop_app_support.hpp"
#include "desktop_web_server.hpp"

#ifndef MOL_DEVELOPMENT_WEB_ROOT
#define MOL_DEVELOPMENT_WEB_ROOT ""
#endif

namespace {

int g_exit_code = 0;

std::filesystem::path NativePath(const wxString& value) {
#if defined(_WIN32)
  return std::filesystem::path(value.ToStdWstring());
#else
  return std::filesystem::path(value.ToStdString());
#endif
}

wxString WxPath(const std::filesystem::path& value) {
#if defined(_WIN32)
  return wxString(value.wstring());
#else
  return wxString::FromUTF8(value.string());
#endif
}

struct Options {
  std::filesystem::path web_root;
  std::filesystem::path acceptance_output;
};

bool ParseOptions(int count, wxChar** values, Options& options, wxString& error) {
  for (int index = 1; index < count; ++index) {
    const wxString argument(values[index]);
    if (argument == "--web-root" || argument == "--acceptance-output") {
      if (++index >= count) {
        error = argument + " requires a path.";
        return false;
      }
      (argument == "--web-root" ? options.web_root : options.acceptance_output) =
          NativePath(values[index]);
    } else {
      error = "Unknown option: " + argument;
      return false;
    }
  }
  return true;
}

class WebFrame final : public wxFrame {
 public:
  WebFrame(std::uint16_t port, std::filesystem::path acceptance_output)
      : wxFrame(nullptr, wxID_ANY, "MoL Keyboard", wxDefaultPosition, wxSize(1180, 760)),
        port_(port),
        acceptance_output_(std::move(acceptance_output)),
        acceptance_timer_(this) {}

  bool Initialize(const wxString& url, const wxString& backend) {
    // wxWidgets 3.2's Edge backend stores its profile below the application-specific
    // user-local-data directory returned by wxStandardPaths.
    browser_ = wxWebView::New(backend);
    if (browser_ == nullptr ||
        !browser_->Create(this, wxID_ANY, "about:blank", wxDefaultPosition, wxDefaultSize)) {
      delete browser_;
      browser_ = nullptr;
      return false;
    }
    browser_->EnableContextMenu(false);
    browser_->EnableAccessToDevTools(false);
    browser_->EnableHistory(false);
    browser_->Bind(wxEVT_WEBVIEW_NAVIGATING, &WebFrame::OnNavigating, this);
    browser_->Bind(wxEVT_WEBVIEW_NEWWINDOW, &WebFrame::OnNewWindow, this);
    browser_->Bind(wxEVT_WEBVIEW_LOADED, &WebFrame::OnLoaded, this);
    browser_->Bind(wxEVT_WEBVIEW_ERROR, &WebFrame::OnError, this);
    Bind(wxEVT_TIMER, &WebFrame::OnAcceptanceTimeout, this, acceptance_timer_.GetId());
    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(browser_, 1, wxEXPAND);
    SetSizer(layout);
    SetMinSize(wxSize(760, 540));
    browser_->LoadURL(url);
    return true;
  }

 private:
  void FinishAcceptance(bool passed, const wxString& detail) {
    if (acceptance_finished_) return;
    acceptance_finished_ = true;
    acceptance_timer_.Stop();
    std::ofstream output(acceptance_output_, std::ios::binary);
    output << "passed=" << (passed ? "true" : "false") << '\n'
           << "detail=" << detail.ToStdString() << '\n'
           << "url=" << browser_->GetCurrentURL().ToStdString() << '\n';
    if (!output.good()) passed = false;
    g_exit_code = passed ? 0 : 1;
    CallAfter([this]() {
      Destroy();
      if (wxTheApp != nullptr) wxTheApp->ExitMainLoop();
    });
  }

  void OnNavigating(wxWebViewEvent& event) {
    const std::string url = event.GetURL().ToStdString();
    const std::string acceptance_prefix = "http://127.0.0.1:" + std::to_string(port_) +
                                          "/__mol_desktop_acceptance__/";
    if (!acceptance_output_.empty() && url.rfind(acceptance_prefix, 0u) == 0u) {
      event.Veto();
      const std::string result = url.substr(acceptance_prefix.size());
      FinishAcceptance(result.rfind("PASS-", 0u) == 0u,
                       wxString::FromUTF8("web capabilities=" + result));
    } else if (!moldesktop::IsTrustedNavigation(url, port_)) {
      event.Veto();
      if (!acceptance_output_.empty()) FinishAcceptance(false, "blocked navigation");
    }
  }

  void OnNewWindow(wxWebViewEvent& event) {
    event.Veto();
    if (!acceptance_output_.empty()) FinishAcceptance(false, "blocked new window");
  }

  void OnLoaded(wxWebViewEvent& event) {
    if (acceptance_output_.empty()) return;
    const std::string loaded_url = event.GetURL().ToStdString();
    const std::string expected_origin =
        "http://127.0.0.1:" + std::to_string(port_) + "/";
    if (loaded_url.rfind(expected_origin, 0u) != 0u || acceptance_started_) return;
    acceptance_started_ = true;
    acceptance_timer_.StartOnce(10000);
  }

  void OnAcceptanceTimeout(wxTimerEvent&) {
    FinishAcceptance(false, "capability report timed out");
  }

  void OnError(wxWebViewEvent& event) {
    const wxString detail = "WebView load failed: " + event.GetString();
    if (!acceptance_output_.empty()) {
      FinishAcceptance(false, detail);
    } else {
      wxMessageBox(detail, "MoL Keyboard", wxOK | wxICON_ERROR, this);
    }
  }

  wxWebView* browser_ = nullptr;
  std::uint16_t port_ = 0;
  std::filesystem::path acceptance_output_;
  wxTimer acceptance_timer_;
  bool acceptance_started_ = false;
  bool acceptance_finished_ = false;
};

}  // namespace

class DesktopApp final : public wxApp {
 public:
  bool OnInit() override {
    SetAppName("MoL Keyboard");
    SetVendorName("MoL Keyboard contributors");
    Options options;
    wxString option_error;
    if (!ParseOptions(argc, argv, options, option_error)) return Fail(option_error, options);

    if (options.acceptance_output.empty()) {
      instance_ = std::make_unique<wxSingleInstanceChecker>(
          "cn.zhangpeixuan.molkeyboard.desktop-ui");
      if (instance_->IsAnotherRunning())
        return Fail("MoL Keyboard is already running.", options);
    }

    const auto executable = NativePath(wxStandardPaths::Get().GetExecutablePath());
    const auto resources = NativePath(wxStandardPaths::Get().GetResourcesDir());
    const std::filesystem::path web_root = moldesktop::FindWebRoot(
        options.web_root,
        {resources / "web", executable.parent_path().parent_path() / "share/mol-keyboard/web",
         std::filesystem::path(MOL_DEVELOPMENT_WEB_ROOT)});
    std::string server_error;
    if (!server_.Start(web_root, 0, server_error))
      return Fail("The local interface could not start.\n\n" + wxString::FromUTF8(server_error) +
                      "\n\nWeb UI: " + WxPath(web_root),
                  options);

#if defined(__WXMSW__)
    const wxString backend = wxWebViewBackendEdge;
#else
    const wxString backend = wxWebViewBackendDefault;
#endif
    if (!wxWebView::IsBackendAvailable(backend))
      return Fail("The required system WebView runtime is unavailable.", options);

    wxString url = wxString::Format("http://127.0.0.1:%u/", server_.port());
    if (!options.acceptance_output.empty()) url += "?mol-desktop-acceptance";
    auto* frame = new WebFrame(server_.port(), options.acceptance_output);
    if (!frame->Initialize(url, backend)) {
      frame->Destroy();
      return Fail("The native WebView could not be created.", options);
    }
    frame->Show();
    SetTopWindow(frame);
    return true;
  }

  int OnExit() override {
    server_.Stop();
    instance_.reset();
    return g_exit_code;
  }

  int OnRun() override {
    (void)wxApp::OnRun();
    return g_exit_code;
  }

 private:
  bool Fail(const wxString& message, const Options& options) {
    g_exit_code = 1;
    if (!options.acceptance_output.empty()) {
      std::ofstream output(options.acceptance_output, std::ios::binary);
      output << "passed=false\ndetail=" << message.ToStdString() << '\n';
    } else {
      wxMessageBox(message, "MoL Keyboard", wxOK | wxICON_ERROR);
    }
    return false;
  }

  moldesktop::WebServer server_;
  std::unique_ptr<wxSingleInstanceChecker> instance_;
};

wxIMPLEMENT_APP(DesktopApp);
