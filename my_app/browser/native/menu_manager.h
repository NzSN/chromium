// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MY_APP_BROWSER_NATIVE_MENU_MANAGER_H_
#define MY_APP_BROWSER_NATIVE_MENU_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "my_app/common/mojom/native_api.mojom.h"
#include "ui/menus/simple_menu_model.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

namespace my_app {

using ContextMenuCallback = base::OnceCallback<void(int32_t)>;

class MenuManager : public ui::SimpleMenuModel::Delegate {
 public:
  static void ShowContextMenu(std::vector<mojom::MenuItemPtr> items,
                              views::Widget* widget,
                              const gfx::Point& point,
                              ContextMenuCallback callback);
  ~MenuManager() override;

 private:
  explicit MenuManager(ContextMenuCallback callback);

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  ContextMenuCallback callback_;
  int32_t selected_id_ = -1;
  base::flat_set<int> checked_ids_;
};

}  // namespace my_app

#endif  // MY_APP_BROWSER_NATIVE_MENU_MANAGER_H_
