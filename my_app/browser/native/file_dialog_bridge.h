// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_NATIVE_FILE_DIALOG_BRIDGE_H_
#define MY_APP_BROWSER_NATIVE_FILE_DIALOG_BRIDGE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace my_app {

using OpenCallback =
    base::OnceCallback<void(const std::vector<std::string>&)>;
using SaveCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;

class FileDialogBridge : public ui::SelectFileDialog::Listener {
 public:
  static void ShowOpen(const std::string& title,
                       const std::vector<std::string>& extensions,
                       gfx::NativeWindow parent_window,
                       OpenCallback callback);

  static void ShowSave(const std::string& title,
                       const std::string& default_name,
                       const std::vector<std::string>& extensions,
                       gfx::NativeWindow parent_window,
                       SaveCallback callback);

 private:
  explicit FileDialogBridge(OpenCallback callback);
  explicit FileDialogBridge(SaveCallback callback);
  ~FileDialogBridge() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

  scoped_refptr<ui::SelectFileDialog> dialog_;
  OpenCallback open_callback_;
  SaveCallback save_callback_;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_NATIVE_FILE_DIALOG_BRIDGE_H_
