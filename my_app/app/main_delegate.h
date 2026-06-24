// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_APP_MAIN_DELEGATE_H_
#define MY_APP_APP_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "content/public/app/content_main_delegate.h"

namespace content {
class ContentClient;
class ContentBrowserClient;
class ContentRendererClient;
}  // namespace content

namespace my_app {

class AppMainDelegate : public content::ContentMainDelegate {
 public:
  AppMainDelegate();
  ~AppMainDelegate() override;

 private:
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
};

}  // namespace my_app

#endif  // MY_APP_APP_MAIN_DELEGATE_H_
