// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_APP_URL_LOADER_FACTORY_H_
#define MY_APP_BROWSER_APP_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

namespace my_app {

class AppURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

  AppURLLoaderFactory(const AppURLLoaderFactory&) = delete;
  AppURLLoaderFactory& operator=(const AppURLLoaderFactory&) = delete;

 private:
  explicit AppURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);
  ~AppURLLoaderFactory() override;

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_APP_URL_LOADER_FACTORY_H_
