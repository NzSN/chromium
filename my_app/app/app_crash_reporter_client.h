// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_APP_CRASH_REPORTER_CLIENT_H_
#define MY_APP_APP_CRASH_REPORTER_CLIENT_H_

#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace my_app {

class AppCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  AppCrashReporterClient();

  AppCrashReporterClient(const AppCrashReporterClient&) = delete;
  AppCrashReporterClient& operator=(const AppCrashReporterClient&) = delete;

  ~AppCrashReporterClient() override;

#if BUILDFLAG(IS_WIN)
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  base::FilePath GetReporterLogFilename() override;
#endif

#if BUILDFLAG(IS_WIN)
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
#else
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif

  void GetProductInfo(ProductInfo* product_info) override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;
};

}  // namespace my_app

#endif  // MY_APP_APP_CRASH_REPORTER_CLIENT_H_
