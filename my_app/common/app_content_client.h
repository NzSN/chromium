// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_COMMON_APP_CONTENT_CLIENT_H_
#define MY_APP_COMMON_APP_CONTENT_CLIENT_H_

#include <string>
#include <string_view>

#include "content/public/common/content_client.h"

namespace my_app {

class AppContentClient : public content::ContentClient {
 public:
  AppContentClient();
  ~AppContentClient() override;

  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
};

}  // namespace my_app

#endif  // MY_APP_COMMON_APP_CONTENT_CLIENT_H_
