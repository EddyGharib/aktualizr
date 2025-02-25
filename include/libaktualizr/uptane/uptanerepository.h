#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <cstdint>               // for int64_t
#include <string>                // for string
#include "libaktualizr/types.h"  // for TimeStamp
#include "libaktualizr/uptane/tuf.h"          // for Root, RepositoryType

class INvStorage;

namespace Uptane {
class IMetadataFetcher;

class RepositoryCommon {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  RepositoryCommon(RepositoryType type_in) : type{type_in} {}
  virtual ~RepositoryCommon() = default;
  RepositoryCommon(const RepositoryCommon &guard) = default;
  RepositoryCommon(RepositoryCommon &&) = default;
  RepositoryCommon &operator=(const RepositoryCommon &guard) = default;
  RepositoryCommon &operator=(RepositoryCommon &&) = default;
  void initRoot(RepositoryType repo_type, const std::string &root_raw);
  void verifyRoot(const std::string &root_raw);
  int rootVersion() const { return root.version(); }
  bool rootExpired() const { return root.isExpired(TimeStamp::Now()); }
  virtual void updateMeta(INvStorage &storage, const IMetadataFetcher &fetcher) = 0;

 protected:
  void resetRoot();
  void updateRoot(INvStorage &storage, const IMetadataFetcher &fetcher, RepositoryType repo_type);

  static const int64_t kMaxRotations = 1000;

  Root root;
  RepositoryType type;
};
}  // namespace Uptane

#endif
