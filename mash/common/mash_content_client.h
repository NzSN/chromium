#ifndef MASH_COMMON_MASH_CONTENT_CLIENT_H_
#define MASH_COMMON_MASH_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"

namespace mash {

class MashContentClient : public content::ContentClient {
 public:
  MashContentClient();
  ~MashContentClient() override;

  // ContentClient overrides:
  void AddAdditionalSchemes(Schemes* schemes) override;
};

}  // namespace mash

#endif  // MASH_COMMON_MASH_CONTENT_CLIENT_H_
