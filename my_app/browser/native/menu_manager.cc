// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/native/menu_manager.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace my_app {

// static
void MenuManager::ShowContextMenu(std::vector<mojom::MenuItemPtr> items,
                                  views::Widget* widget,
                                  const gfx::Point& point,
                                  ContextMenuCallback callback) {
  if (!widget) {
    std::move(callback).Run(-1);
    return;
  }

  auto* manager = new MenuManager(std::move(callback));
  manager->menu_model_ = std::make_unique<ui::SimpleMenuModel>(manager);

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    if (item->checked) {
      manager->menu_model_->AddCheckItem(item->id,
                                         base::UTF8ToUTF16(item->label));
      manager->checked_ids_.insert(item->id);
    } else {
      manager->menu_model_->AddItem(item->id,
                                    base::UTF8ToUTF16(item->label));
    }
    manager->menu_model_->SetEnabledAt(i, item->enabled);
  }

  manager->menu_runner_ = std::make_unique<views::MenuRunner>(
      manager->menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  manager->menu_runner_->RunMenuAt(
      widget, nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft,
      ui::mojom::MenuSourceType::kMouse);
}

MenuManager::MenuManager(ContextMenuCallback callback)
    : callback_(std::move(callback)) {}

MenuManager::~MenuManager() = default;

void MenuManager::ExecuteCommand(int command_id, int event_flags) {
  selected_id_ = command_id;
}

bool MenuManager::IsCommandIdChecked(int command_id) const {
  return checked_ids_.contains(command_id);
}

bool MenuManager::IsCommandIdEnabled(int command_id) const {
  return true;
}

void MenuManager::MenuClosed(ui::SimpleMenuModel* source) {
  std::move(callback_).Run(selected_id_);
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::unique_ptr<MenuManager>(this));
}

}  // namespace my_app
