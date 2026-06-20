// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/app_url_loader_factory.h"

#include <string>

#include "base/containers/span.h"
#include "grit/my_app_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace my_app {

namespace {

struct ResourceEntry {
  const char* path;
  int resource_id;
  const char* mime_type;
};

const ResourceEntry kResources[] = {
    {"/index.html", IDR_MY_APP_INDEX_HTML, "text/html"},
    {"/app.js", IDR_MY_APP_APP_JS, "application/javascript"},
    {"/app.css", IDR_MY_APP_APP_CSS, "text/css"},
};

const ResourceEntry* FindResource(const std::string& path) {
  for (const auto& entry : kResources) {
    if (path == entry.path) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
AppURLLoaderFactory::Create() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  new AppURLLoaderFactory(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

AppURLLoaderFactory::AppURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)) {}

AppURLLoaderFactory::~AppURLLoaderFactory() = default;

void AppURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));

  std::string path(request.url.path());
  if (path.empty() || path == "/") {
    path = "/index.html";
  }

  const ResourceEntry* entry = FindResource(path);
  if (!entry) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
    return;
  }

  std::string_view raw_data =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          entry->resource_id);
  if (raw_data.empty()) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
    return;
  }

  std::string body;
  if (raw_data.size() >= 2 &&
      static_cast<uint8_t>(raw_data[0]) == 0x1f &&
      static_cast<uint8_t>(raw_data[1]) == 0x8b) {
    if (!compression::GzipUncompress(std::string(raw_data), &body)) {
      client_remote->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }
  } else {
    body = std::string(raw_data);
  }

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::Builder(
      {1, 1}, "200 OK").Build();
  response_head->mime_type = entry->mime_type;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (mojo::CreateDataPipe(body.size(), producer_handle,
                           consumer_handle) != MOJO_RESULT_OK) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  base::span<uint8_t> buffer;
  MojoResult result = producer_handle->BeginWriteData(
      body.size(), MOJO_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE, buffer);
  if (result != MOJO_RESULT_OK) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  buffer.first(body.size()).copy_from(base::as_byte_span(body));
  producer_handle->EndWriteData(body.size());

  client_remote->OnReceiveResponse(std::move(response_head),
                                   std::move(consumer_handle), std::nullopt);

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = body.size();
  status.encoded_body_length = body.size();
  client_remote->OnComplete(status);
}

}  // namespace my_app
