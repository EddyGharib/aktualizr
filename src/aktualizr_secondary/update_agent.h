#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_H

#include "libaktualizr/crypto/crypto.h"
#include "libaktualizr/uptane/tuf.h"

class UpdateAgent {
 public:
  using Ptr = std::shared_ptr<UpdateAgent>;

  virtual ~UpdateAgent() = default;
  UpdateAgent(const UpdateAgent&) = delete;
  UpdateAgent(UpdateAgent&&) = delete;
  UpdateAgent& operator=(const UpdateAgent&) = delete;
  UpdateAgent& operator=(UpdateAgent&&) = delete;

  virtual bool isTargetSupported(const Uptane::Target& target) const = 0;
  virtual bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const = 0;
  virtual data::InstallationResult install(const Uptane::Target& target) = 0;

  virtual void completeInstall() = 0;
  virtual data::InstallationResult applyPendingInstall(const Uptane::Target& target) = 0;

 protected:
  UpdateAgent() = default;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_H
