#ifndef MASH_COMMON_MASH_PATHS_H_
#define MASH_COMMON_MASH_PATHS_H_

namespace mash {

enum MashPathKey {
  MASH_PATH_START = 2000,
  MASH_DIR_USER_DATA = MASH_PATH_START,
  MASH_PATH_END,
};

void RegisterMashPathProvider();

}  // namespace mash

#endif  // MASH_COMMON_MASH_PATHS_H_
