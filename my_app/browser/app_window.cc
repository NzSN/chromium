// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/app_window.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace my_app {

namespace {
base::RepeatingClosure& GetQuitClosure() {
  static base::NoDestructor<base::RepeatingClosure> closure;
  return *closure;
}
}  // namespace

AppWindow::AppWindow(content::BrowserContext* context) {
  content::WebContents::CreateParams params(context);
  web_contents_ = content::WebContents::Create(params);
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  SetHasWindowSizeControls(true);
  SetTitle(u"MyApp");

  auto web_view = std::make_unique<views::WebView>(context);
  web_view->SetWebContents(web_contents_.get());
  web_view_ = SetContentsView(std::move(web_view));
}

AppWindow::~AppWindow() {
  auto& windows = GetWindows();
  std::erase(windows, this);

  if (windows.empty() && GetQuitClosure()) {
    GetQuitClosure().Run();
  }
}

// static
AppWindow* AppWindow::Create(content::BrowserContext* context,
                             const GURL& url) {
  auto* window = new AppWindow(context);
  GetWindows().push_back(window);

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = window;
  params.bounds = gfx::Rect(800, 600);

  window->widget_ = std::make_unique<views::Widget>();
  window->widget_->Init(std::move(params));
  window->widget_->Show();

  window->LoadURL(url);
  return window;
}

// static
void AppWindow::CloseAllWindows() {
  while (!GetWindows().empty()) {
    auto* window = GetWindows().back();
    window->web_view_ = nullptr;
    window->widget_.reset();
    delete window;
  }
}

// static
std::vector<AppWindow*>& AppWindow::GetWindows() {
  static base::NoDestructor<std::vector<AppWindow*>> windows;
  return *windows;
}

// static
void AppWindow::SetQuitClosure(base::RepeatingClosure quit_closure) {
  GetQuitClosure() = std::move(quit_closure);
}

void AppWindow::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void AppWindow::WidgetIsZombie(views::Widget* widget) {
  web_view_ = nullptr;
  widget_.reset();
  delete this;
}

void AppWindow::CloseContents(content::WebContents* source) {
  if (widget_) {
    widget_->Close();
  }
}

void AppWindow::TitleWasSet(content::NavigationEntry* entry) {
  if (widget_ && web_contents_) {
    SetTitle(web_contents_->GetTitle());
    widget_->UpdateWindowTitle();
  }
}

}  // namespace my_app
