// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_APP_BROWSER_MAIN_PARTS_H_
#define MY_APP_BROWSER_APP_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "ui/display/screen.h"

namespace views {
class ViewsDelegate;
}  // namespace views

namespace wm {
class WMState;
}  // namespace wm

namespace my_app {

class AppBrowserContext;

class AppBrowserMainParts : public content::BrowserMainParts {
 public:
  AppBrowserMainParts();
  ~AppBrowserMainParts() override;

  AppBrowserContext* browser_context() { return browser_context_.get(); }

 private:
  int PreCreateThreads() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  std::unique_ptr<wm::WMState> wm_state_;
  std::unique_ptr<display::Screen> screen_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<AppBrowserContext> browser_context_;
  base::RepeatingClosure quit_closure_;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_APP_BROWSER_MAIN_PARTS_H_
