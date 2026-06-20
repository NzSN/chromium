#include "mash/common/mash_content_client.h"

namespace mash {

MashContentClient::MashContentClient() = default;
MashContentClient::~MashContentClient() = default;

void MashContentClient::AddAdditionalSchemes(Schemes* schemes) {
  // Register custom schemes here as needed.
}

}  // namespace mash
