// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/app/main_delegate.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/path_service.h"
#include "content/public/common/content_client.h"
#include "my_app/browser/app_browser_client.h"
#include "my_app/common/app_content_client.h"
#include "my_app/renderer/app_renderer_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace my_app {

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
