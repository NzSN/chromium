// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/native/notification_manager.h"

#include <memory>
#include <string>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace my_app {

// static
void NotificationManager::Initialize() {
  message_center::MessageCenter::Initialize();
}

// static
void NotificationManager::Shutdown() {
  message_center::MessageCenter::Shutdown();
}

// static
std::string NotificationManager::Show(const std::string& title,
                                      const std::string& body) {
  std::string id =
      "my_app_notification_" + base::NumberToString(base::RandUint64());

  message_center::RichNotificationData optional_fields;
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      base::UTF8ToUTF16(title), base::UTF8ToUTF16(body),
      ui::ImageModel(), u"my_app", GURL(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 "my_app"),
      optional_fields, nullptr);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return id;
}

}  // namespace my_app
