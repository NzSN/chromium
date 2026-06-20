// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/native/clipboard_bridge.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace my_app {

// static
void ClipboardBridge::ReadText(
    base::OnceCallback<void(const std::string&)> callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadText(
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/std::nullopt,
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&)> cb,
             std::u16string result) {
            std::string utf8 = base::UTF16ToUTF8(result);
            std::move(cb).Run(utf8);
          },
          std::move(callback)));
}

// static
void ClipboardBridge::WriteText(const std::string& text) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteText(base::UTF8ToUTF16(text));
}

// static
void ClipboardBridge::ReadHtml(
    base::OnceCallback<void(const std::string&)> callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/std::nullopt,
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&)> cb,
             std::u16string markup, GURL /*src_url*/,
             uint32_t /*fragment_start*/, uint32_t /*fragment_end*/) {
            std::string utf8 = base::UTF16ToUTF8(markup);
            std::move(cb).Run(utf8);
          },
          std::move(callback)));
}

}  // namespace my_app
