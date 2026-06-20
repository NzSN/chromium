// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/native/file_dialog_bridge.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace my_app {

namespace {

ui::SelectFileDialog::FileTypeInfo BuildFileTypeInfo(
    const std::vector<std::string>& extensions) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  if (!extensions.empty()) {
    std::vector<base::FilePath::StringType> ext_list;
    for (const auto& ext : extensions) {
      ext_list.push_back(base::FilePath::FromUTF8Unsafe(ext).value());
    }
    file_type_info.extensions.push_back(std::move(ext_list));
  }
  file_type_info.include_all_files = true;
  return file_type_info;
}

}  // namespace

// static
void FileDialogBridge::ShowOpen(const std::string& title,
                                const std::vector<std::string>& extensions,
                                gfx::NativeWindow parent_window,
                                OpenCallback callback) {
  auto* bridge = new FileDialogBridge(std::move(callback));
  bridge->dialog_ = ui::SelectFileDialog::Create(bridge, nullptr);
  auto file_type_info = BuildFileTypeInfo(extensions);
  bridge->dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE,
      base::UTF8ToUTF16(title), base::FilePath(), &file_type_info, 0,
      base::FilePath::StringType(), parent_window);
}

// static
void FileDialogBridge::ShowSave(const std::string& title,
                                const std::string& default_name,
                                const std::vector<std::string>& extensions,
                                gfx::NativeWindow parent_window,
                                SaveCallback callback) {
  auto* bridge = new FileDialogBridge(std::move(callback));
  bridge->dialog_ = ui::SelectFileDialog::Create(bridge, nullptr);
  auto file_type_info = BuildFileTypeInfo(extensions);
  base::FilePath default_path =
      base::FilePath::FromUTF8Unsafe(default_name);
  bridge->dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      base::UTF8ToUTF16(title), default_path, &file_type_info, 0,
      base::FilePath::StringType(), parent_window);
}

FileDialogBridge::FileDialogBridge(OpenCallback callback)
    : open_callback_(std::move(callback)) {}

FileDialogBridge::FileDialogBridge(SaveCallback callback)
    : save_callback_(std::move(callback)) {}

FileDialogBridge::~FileDialogBridge() = default;

void FileDialogBridge::FileSelected(const ui::SelectedFileInfo& file,
                                    int index) {
  if (save_callback_) {
    std::move(save_callback_).Run(file.path().AsUTF8Unsafe());
  } else if (open_callback_) {
    std::vector<std::string> paths;
    paths.push_back(file.path().AsUTF8Unsafe());
    std::move(open_callback_).Run(std::move(paths));
  }
  delete this;
}

void FileDialogBridge::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  if (open_callback_) {
    std::vector<std::string> paths;
    paths.reserve(files.size());
    for (const auto& file : files) {
      paths.push_back(file.path().AsUTF8Unsafe());
    }
    std::move(open_callback_).Run(std::move(paths));
  }
  delete this;
}

void FileDialogBridge::FileSelectionCanceled() {
  if (save_callback_) {
    std::move(save_callback_).Run(std::nullopt);
  } else if (open_callback_) {
    std::move(open_callback_).Run(std::vector<std::string>());
  }
  delete this;
}

}  // namespace my_app
