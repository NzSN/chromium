// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_APP_BROWSER_CLIENT_H_
#define MY_APP_BROWSER_APP_BROWSER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace url {
class Origin;
}  // namespace url

namespace my_app {

class AppBrowserClient : public content::ContentBrowserClient {
 public:
  AppBrowserClient();
  ~AppBrowserClient() override;

 private:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::string GetUserAgent() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      content::FrameTreeNodeId frame_tree_node_id) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_APP_BROWSER_CLIENT_H_
