#ifndef AKTUALIZR_SECONDARY_METADATA_H_
#define AKTUALIZR_SECONDARY_METADATA_H_

#include "libaktualizr/uptane/fetcher.h"
#include "libaktualizr/uptane/tuf.h"

namespace Uptane {

class SecondaryMetadata : public IMetadataFetcher {
 public:
  explicit SecondaryMetadata(MetaBundle meta_bundle_in);

  void fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Role& role,
                 Version version) const override;
  void fetchLatestRole(std::string* result, int64_t maxsize, RepositoryType repo, const Role& role) const override;

 protected:
  virtual void getRoleMetadata(std::string* result, const RepositoryType& repo, const Role& role,
                               Version version) const;

 private:
  const MetaBundle meta_bundle_;
  Version director_root_version_;
  Version image_root_version_;
};

}  // namespace Uptane

#endif  // AKTUALIZR_SECONDARY_METADATA_H_
