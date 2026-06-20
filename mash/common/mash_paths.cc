#include "mash/common/mash_paths.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "mash/common/mash_switches.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/nix/xdg_util.h"
#endif

namespace mash {

namespace {

bool GetDefaultUserDirectory(base::FilePath* result) {
#if BUILDFLAG(IS_WIN)
  CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, result));
  *result = result->Append(L"mash");
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append("mash");
#elif BUILDFLAG(IS_APPLE)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, result));
  *result = result->Append("Mash");
#else
  return false;
#endif
  return true;
}

}  // namespace

bool MashPathProvider(int key, base::FilePath* result) {
  switch (key) {
    case MASH_DIR_USER_DATA: {
      base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
      if (cmd_line->HasSwitch(switches::kMashUserDataDir)) {
        base::FilePath path =
            cmd_line->GetSwitchValuePath(switches::kMashUserDataDir);
        if (base::DirectoryExists(path) || base::CreateDirectory(path)) {
          if (!path.IsAbsolute()) {
            path = base::MakeAbsoluteFilePath(path);
          }
          if (!path.empty()) {
            *result = path;
            return true;
          }
        }
      }
      bool rv = GetDefaultUserDirectory(result);
      if (rv) {
        base::ScopedAllowBlocking allow_io;
        if (!base::PathExists(*result)) {
          base::CreateDirectory(*result);
        }
      }
      return rv;
    }
    default:
      return false;
  }
}

void RegisterMashPathProvider() {
  base::PathService::RegisterProvider(MashPathProvider, MASH_PATH_START,
                                      MASH_PATH_END);
}

}  // namespace mash
