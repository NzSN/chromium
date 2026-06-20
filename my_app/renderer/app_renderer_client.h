// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_RENDERER_APP_RENDERER_CLIENT_H_
#define MY_APP_RENDERER_APP_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace my_app {

class AppRendererClient : public content::ContentRendererClient {
 public:
  AppRendererClient();
  ~AppRendererClient() override;

  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
};

}  // namespace my_app

#endif  // MY_APP_RENDERER_APP_RENDERER_CLIENT_H_
