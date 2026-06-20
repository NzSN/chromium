// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "my_app/browser/app_browser_context.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace my_app {

AppBrowserContext::AppBrowserContext() {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  path_ = home_dir.Append(FILE_PATH_LITERAL(".config"))
              .Append(FILE_PATH_LITERAL("MyApp"));
  base::CreateDirectory(path_);
}

AppBrowserContext::~AppBrowserContext() {
  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();
}

std::unique_ptr<content::ZoomLevelDelegate>
AppBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath AppBrowserContext::GetPath() const {
  return path_;
}

bool AppBrowserContext::IsOffTheRecord() {
  return false;
}

content::DownloadManagerDelegate*
AppBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager* AppBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* AppBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
AppBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* AppBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
AppBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* AppBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
AppBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
AppBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

content::ClientHintsControllerDelegate*
AppBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
AppBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AppBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AppBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

}  // namespace my_app
