// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_NATIVE_NOTIFICATION_MANAGER_H_
#define MY_APP_BROWSER_NATIVE_NOTIFICATION_MANAGER_H_

#include <string>

namespace my_app {

class NotificationManager {
 public:
  NotificationManager() = delete;

  static void Initialize();
  static void Shutdown();
  static std::string Show(const std::string& title, const std::string& body);
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_NATIVE_NOTIFICATION_MANAGER_H_
