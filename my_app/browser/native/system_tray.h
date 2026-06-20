// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_NATIVE_SYSTEM_TRAY_H_
#define MY_APP_BROWSER_NATIVE_SYSTEM_TRAY_H_

#include <string>

namespace my_app {

class SystemTray {
 public:
  virtual ~SystemTray();

  static SystemTray* GetInstance();

  virtual void SetTooltip(const std::string& tooltip) = 0;
  virtual void SetVisible(bool visible) = 0;

 protected:
  SystemTray();
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_NATIVE_SYSTEM_TRAY_H_
