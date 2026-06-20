// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/renderer/app_renderer_client.h"

namespace my_app {

AppRendererClient::AppRendererClient() = default;
AppRendererClient::~AppRendererClient() = default;

void AppRendererClient::RenderThreadStarted() {}

void AppRendererClient::RenderFrameCreated(content::RenderFrame* render_frame) {
}

}  // namespace my_app
