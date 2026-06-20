// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_IPC_NATIVE_API_IMPL_H_
#define MY_APP_BROWSER_IPC_NATIVE_API_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "my_app/common/mojom/native_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace my_app {

class NativeApiImpl : public mojom::NativeApi {
 public:
  explicit NativeApiImpl(content::RenderFrameHost* render_frame_host);
  ~NativeApiImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::NativeApi> receiver);

  // mojom::NativeApi:
  void ReadClipboardText(ReadClipboardTextCallback callback) override;
  void WriteClipboardText(const std::string& text) override;
  void ReadClipboardHtml(ReadClipboardHtmlCallback callback) override;
  void ShowOpenFileDialog(const std::string& title,
                          const std::vector<std::string>& extensions,
                          ShowOpenFileDialogCallback callback) override;
  void ShowSaveFileDialog(const std::string& title,
                          const std::string& default_name,
                          const std::vector<std::string>& extensions,
                          ShowSaveFileDialogCallback callback) override;
  void ShowNotification(const std::string& title,
                        const std::string& body,
                        ShowNotificationCallback callback) override;
  void SetSystemTrayTooltip(const std::string& tooltip) override;
  void SetSystemTrayVisible(bool visible) override;
  void ShowContextMenu(std::vector<mojom::MenuItemPtr> items,
                       ShowContextMenuCallback callback) override;

 private:
  raw_ptr<content::RenderFrameHost> render_frame_host_;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_IPC_NATIVE_API_IMPL_H_
