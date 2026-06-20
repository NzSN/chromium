// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/ipc/native_api_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "my_app/browser/native/clipboard_bridge.h"
#include "my_app/browser/native/file_dialog_bridge.h"
#include "my_app/browser/native/menu_manager.h"
#include "my_app/browser/native/notification_manager.h"
#include "my_app/browser/native/system_tray.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace my_app {

NativeApiImpl::NativeApiImpl(content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}
NativeApiImpl::~NativeApiImpl() = default;

// static
void NativeApiImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::NativeApi> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NativeApiImpl>(render_frame_host),
      std::move(receiver));
}

void NativeApiImpl::ReadClipboardText(ReadClipboardTextCallback callback) {
  ClipboardBridge::ReadText(std::move(callback));
}

void NativeApiImpl::WriteClipboardText(const std::string& text) {
  ClipboardBridge::WriteText(text);
}

void NativeApiImpl::ReadClipboardHtml(ReadClipboardHtmlCallback callback) {
  ClipboardBridge::ReadHtml(std::move(callback));
}

void NativeApiImpl::ShowOpenFileDialog(
    const std::string& title,
    const std::vector<std::string>& extensions,
    ShowOpenFileDialogCallback callback) {
  gfx::NativeWindow parent_window = gfx::NativeWindow();
  if (render_frame_host_ && render_frame_host_->GetNativeView()) {
    views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
        render_frame_host_->GetNativeView());
    if (widget) {
      parent_window = widget->GetNativeWindow();
    }
  }
  FileDialogBridge::ShowOpen(title, extensions, parent_window,
                             std::move(callback));
}

void NativeApiImpl::ShowSaveFileDialog(
    const std::string& title,
    const std::string& default_name,
    const std::vector<std::string>& extensions,
    ShowSaveFileDialogCallback callback) {
  gfx::NativeWindow parent_window = gfx::NativeWindow();
  if (render_frame_host_ && render_frame_host_->GetNativeView()) {
    views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
        render_frame_host_->GetNativeView());
    if (widget) {
      parent_window = widget->GetNativeWindow();
    }
  }
  FileDialogBridge::ShowSave(title, default_name, extensions, parent_window,
                             std::move(callback));
}

void NativeApiImpl::ShowNotification(const std::string& title,
                                     const std::string& body,
                                     ShowNotificationCallback callback) {
  std::string id = NotificationManager::Show(title, body);
  std::move(callback).Run(id);
}

void NativeApiImpl::SetSystemTrayTooltip(const std::string& tooltip) {
  SystemTray::GetInstance()->SetTooltip(tooltip);
}

void NativeApiImpl::SetSystemTrayVisible(bool visible) {
  SystemTray::GetInstance()->SetVisible(visible);
}

void NativeApiImpl::ShowContextMenu(std::vector<mojom::MenuItemPtr> items,
                                    ShowContextMenuCallback callback) {
  views::Widget* widget = nullptr;
  if (render_frame_host_ && render_frame_host_->GetNativeView()) {
    widget = views::Widget::GetTopLevelWidgetForNativeView(
        render_frame_host_->GetNativeView());
  }
  gfx::Point point;
  if (display::Screen::Get()) {
    point = display::Screen::Get()->GetCursorScreenPoint();
  }
  MenuManager::ShowContextMenu(std::move(items), widget, point,
                               std::move(callback));
}

}  // namespace my_app
