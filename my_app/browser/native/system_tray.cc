// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/native/system_tray.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace my_app {

namespace {

class SystemTrayStub : public SystemTray {
 public:
  SystemTrayStub() = default;
  ~SystemTrayStub() override = default;

  void SetTooltip(const std::string& tooltip) override {
    LOG(INFO) << "SystemTrayStub::SetTooltip: " << tooltip;
  }

  void SetVisible(bool visible) override {
    LOG(INFO) << "SystemTrayStub::SetVisible: " << visible;
  }
};

}  // namespace

SystemTray::SystemTray() = default;
SystemTray::~SystemTray() = default;

// static
SystemTray* SystemTray::GetInstance() {
  static base::NoDestructor<SystemTrayStub> instance;
  return instance.get();
}

}  // namespace my_app
