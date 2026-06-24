// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/app/app_crash_reporter_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

namespace my_app {

namespace {

base::FilePath GetCrashDumpLocationInternal() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("crash-dumps-dir")) {
    return command_line->GetSwitchValuePath("crash-dumps-dir");
  }
  return base::FilePath();
}

}  // namespace

AppCrashReporterClient::AppCrashReporterClient() {}
AppCrashReporterClient::~AppCrashReporterClient() {}

#if BUILDFLAG(IS_WIN)
void AppCrashReporterClient::GetProductNameAndVersion(
    const std::wstring& exe_path,
    std::wstring* product_name,
    std::wstring* version,
    std::wstring* special_build,
    std::wstring* channel_name) {
  *product_name = L"my_app";
  *version = L"1.0.0.0";
  *special_build = std::wstring();
  *channel_name = std::wstring();
}
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
base::FilePath AppCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

#if BUILDFLAG(IS_WIN)
bool AppCrashReporterClient::GetCrashDumpLocation(std::wstring* crash_dir) {
#else
bool AppCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  base::FilePath crash_directory = GetCrashDumpLocationInternal();
  if (crash_directory.empty()) {
    return false;
  }
#if BUILDFLAG(IS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

void AppCrashReporterClient::GetProductInfo(ProductInfo* product_info) {
  product_info->product_name = "my_app";
  product_info->version = "1.0.0.0";
}

bool AppCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace my_app
