// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_NATIVE_CLIPBOARD_BRIDGE_H_
#define MY_APP_BROWSER_NATIVE_CLIPBOARD_BRIDGE_H_

#include <string>

#include "base/functional/callback.h"

namespace my_app {

class ClipboardBridge {
 public:
  ClipboardBridge() = delete;

  static void ReadText(base::OnceCallback<void(const std::string&)> callback);
  static void WriteText(const std::string& text);
  static void ReadHtml(base::OnceCallback<void(const std::string&)> callback);
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_NATIVE_CLIPBOARD_BRIDGE_H_
