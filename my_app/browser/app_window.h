// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_APP_WINDOW_H_
#define MY_APP_BROWSER_APP_WINDOW_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

namespace my_app {

class AppWindow : public views::WidgetDelegate,
                  public content::WebContentsDelegate,
                  public content::WebContentsObserver {
 public:
  ~AppWindow() override;

  static AppWindow* Create(content::BrowserContext* context, const GURL& url);
  static void CloseAllWindows();
  static std::vector<AppWindow*>& GetWindows();
  static void SetQuitClosure(base::RepeatingClosure quit_closure);

  void LoadURL(const GURL& url);

 private:
  explicit AppWindow(content::BrowserContext* context);

  // views::WidgetDelegate:
  void WidgetIsZombie(views::Widget* widget) override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

  // content::WebContentsObserver:
  void TitleWasSet(content::NavigationEntry* entry) override;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_APP_WINDOW_H_
