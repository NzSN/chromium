// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/app/main_delegate.h"

#include "my_app/app/main_delegate.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/base_switches.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/path_service.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "my_app/app/app_crash_reporter_client.h"
#include "my_app/browser/app_browser_client.h"
#include "my_app/common/app_content_client.h"
#include "my_app/renderer/app_renderer_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace my_app {

namespace {

AppCrashReporterClient* GetCrashReporterClient() {
  static base::NoDestructor<AppCrashReporterClient> client;
  return client.get();
}

void InitializeCrashpadIfEnabled() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    return;
  }

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  crash_reporter::SetCrashReporterClient(GetCrashReporterClient());
  crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
}

}  // namespace

AppMainDelegate::AppMainDelegate() = default;
AppMainDelegate::~AppMainDelegate() = default;

std::optional<int> AppMainDelegate::BasicStartupComplete() {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  CHECK(logging::InitLogging(settings));

  content_client_ = std::make_unique<AppContentClient>();
  content::SetContentClient(content_client_.get());
  return std::nullopt;
}

void AppMainDelegate::PreSandboxStartup() {
  base::FilePath pak_file;
  bool res = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  CHECK(res);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("my_app.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

std::optional<int> AppMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  InitializeCrashpadIfEnabled();
  return std::nullopt;
}

content::ContentBrowserClient* AppMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<AppBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
AppMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<AppRendererClient>();
  return renderer_client_.get();
}

}  // namespace my_app
