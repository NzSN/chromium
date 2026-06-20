// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/app_browser_client.h"

#include "my_app/browser/app_browser_main_parts.h"
#include "my_app/browser/app_url_loader_factory.h"
#include "my_app/browser/ipc/native_api_impl.h"
#include "my_app/common/mojom/native_api.mojom.h"

namespace my_app {

namespace {
const char kMyAppScheme[] = "myapp";
}  // namespace

AppBrowserClient::AppBrowserClient() = default;
AppBrowserClient::~AppBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
AppBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  return std::make_unique<AppBrowserMainParts>();
}

std::string AppBrowserClient::GetUserAgent() {
  return "MyApp/1.0";
}

std::string AppBrowserClient::GetAcceptLangs(content::BrowserContext* context) {
  return "en-US,en";
}

void AppBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<mojom::NativeApi>(base::BindRepeating(&NativeApiImpl::Create));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
AppBrowserClient::CreateNonNetworkNavigationURLLoaderFactory(
    const std::string& scheme,
    content::FrameTreeNodeId frame_tree_node_id) {
  if (scheme == kMyAppScheme) {
    return AppURLLoaderFactory::Create();
  }
  return {};
}

void AppBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const std::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  factories->emplace(kMyAppScheme, AppURLLoaderFactory::Create());
}

}  // namespace my_app
