// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/app_browser_main_parts.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "my_app/browser/app_browser_context.h"
#include "my_app/browser/app_window.h"
#include "my_app/browser/native/notification_manager.h"
#include "my_app/common/app_switches.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/core/wm_state.h"
#include "url/gurl.h"

namespace my_app {

namespace {
class AppViewsDelegate : public views::ViewsDelegate {
 public:
  AppViewsDelegate() = default;
  ~AppViewsDelegate() override = default;

  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (params->native_widget) {
      return;
    }
    if (params->parent &&
        params->type != views::Widget::InitParams::TYPE_MENU &&
        params->type != views::Widget::InitParams::TYPE_TOOLTIP) {
      params->native_widget = new views::NativeWidgetAura(delegate);
    } else {
      params->native_widget = new views::DesktopNativeWidgetAura(delegate);
    }
  }
};
}  // namespace

AppBrowserMainParts::AppBrowserMainParts() = default;
AppBrowserMainParts::~AppBrowserMainParts() = default;

int AppBrowserMainParts::PreCreateThreads() {
  browser_context_ = std::make_unique<AppBrowserContext>();
  NotificationManager::Initialize();
  return 0;
}

int AppBrowserMainParts::PreMainMessageLoopRun() {
  wm_state_ = std::make_unique<wm::WMState>();
  if (!display::Screen::HasScreen()) {
    screen_ = views::CreateDesktopScreen();
  }
  views_delegate_ = std::make_unique<AppViewsDelegate>();

  GURL initial_url("myapp://app/index.html");
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevServer)) {
    initial_url = GURL(command_line->GetSwitchValueASCII(switches::kDevServer));
  }

  AppWindow::Create(browser_context_.get(), initial_url);

  return 0;
}

void AppBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_closure_ = run_loop->QuitClosure();
  AppWindow::SetQuitClosure(quit_closure_);
}

void AppBrowserMainParts::PostMainMessageLoopRun() {
  AppWindow::CloseAllWindows();
  NotificationManager::Shutdown();
  browser_context_.reset();
  views_delegate_.reset();
  screen_.reset();
  wm_state_.reset();
}

}  // namespace my_app
