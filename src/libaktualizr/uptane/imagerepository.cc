#include "libaktualizr/uptane/imagerepository.h"

#include "libaktualizr/crypto/crypto.h"
#include "libaktualizr/logging/logging.h"
#include "libaktualizr/storage/invstorage.h"
#include "libaktualizr/uptane/exceptions.h"
#include "libaktualizr/uptane/fetcher.h"

namespace Uptane {

void ImageRepository::resetMeta() {
  resetRoot();
  targets.reset();
  snapshot = Snapshot();
  timestamp = TimestampMeta();
}

void ImageRepository::verifyTimestamp(const std::string& timestamp_raw) {
  try {
    // Verify the signature:
    timestamp =
        TimestampMeta(RepositoryType::Image(), Utils::parseJSON(timestamp_raw), std::make_shared<MetaWithKeys>(root));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for Timestamp metadata failed";
    throw;
  }
}

void ImageRepository::checkTimestampExpired() {
  if (timestamp.isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.ToString(), Role::TIMESTAMP);
  }
}

void ImageRepository::fetchSnapshot(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string image_snapshot;
  const int64_t snapshot_size = (snapshotSize() > 0) ? snapshotSize() : kMaxSnapshotSize;
  fetcher.fetchLatestRole(&image_snapshot, snapshot_size, RepositoryType::Image(), Role::Snapshot());
  const int remote_version = extractVersionUntrusted(image_snapshot);

  // 6. Check that each Targets metadata filename listed in the previous Snapshot metadata file is also listed in this
  // Snapshot metadata file. If this condition is not met, discard the new Snapshot metadata file, abort the update
  // cycle, and report the failure. (Checks for a rollback attack.)
  // See also https://github.com/uptane/deployment-considerations/pull/39/files.
  // If the Snapshot is rotated, delegations may be safely removed.
  // https://saeljira.it.here.com/browse/OTA-4121
  verifySnapshot(image_snapshot, false);

  if (local_version > remote_version) {
    throw Uptane::SecurityException(RepositoryType::IMAGE, "Rollback attempt");
  } else {
    storage.storeNonRoot(image_snapshot, RepositoryType::Image(), Role::Snapshot());
  }
}

void ImageRepository::verifySnapshot(const std::string& snapshot_raw, bool prefetch) {
  const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(snapshot_raw));
  bool hash_exists = false;
  for (const auto& it : timestamp.snapshot_hashes()) {
    switch (it.type()) {
      case Hash::Type::kSha256:
        if (Hash(Hash::Type::kSha256, Crypto::sha256digestHex(canonical)) != it) {
          if (!prefetch) {
            LOG_ERROR << "Hash verification for Snapshot metadata failed";
          }
          throw Uptane::SecurityException(RepositoryType::IMAGE, "Snapshot metadata hash verification failed");
        }
        hash_exists = true;
        break;
      case Hash::Type::kSha512:
        if (Hash(Hash::Type::kSha512, Crypto::sha512digestHex(canonical)) != it) {
          if (!prefetch) {
            LOG_ERROR << "Hash verification for Snapshot metadata failed";
          }
          throw Uptane::SecurityException(RepositoryType::IMAGE, "Snapshot metadata hash verification failed");
        }
        hash_exists = true;
        break;
      default:
        break;
    }
  }

  if (!hash_exists) {
    LOG_ERROR << "No hash found for snapshot.json";
    throw Uptane::SecurityException(RepositoryType::IMAGE, "Snapshot metadata hash verification failed");
  }

  try {
    // Verify the signature:
    snapshot = Snapshot(RepositoryType::Image(), Utils::parseJSON(snapshot_raw), std::make_shared<MetaWithKeys>(root));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for Snapshot metadata failed";
    throw;
  }

  if (snapshot.version() != timestamp.snapshot_version()) {
    throw Uptane::VersionMismatch(RepositoryType::IMAGE, Uptane::Role::SNAPSHOT);
  }
}

void ImageRepository::checkSnapshotExpired() {
  if (snapshot.isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.ToString(), Role::SNAPSHOT);
  }
}

void ImageRepository::fetchTargets(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string image_targets;
  const Role targets_role = Role::Targets();

  auto targets_size = getRoleSize(Role::Targets());
  if (targets_size <= 0) {
    targets_size = kMaxImageTargetsSize;
  }

  fetcher.fetchLatestRole(&image_targets, targets_size, RepositoryType::Image(), targets_role);

  const int remote_version = extractVersionUntrusted(image_targets);

  verifyTargets(image_targets, false, false);

  if (local_version > remote_version) {
    throw Uptane::SecurityException(RepositoryType::IMAGE, "Rollback attempt");
  } else {
    storage.storeNonRoot(image_targets, RepositoryType::Image(), targets_role);
  }
}

void ImageRepository::verifyRoleHashes(const std::string& role_data, const Uptane::Role& role, bool prefetch) const {
  const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(role_data));
  // Hashes are not required in snapshot metadata. If present, however, we may as well check them.
  // This provides no security benefit, but may help with fault detection.
  for (const auto& it : snapshot.role_hashes(role)) {
    switch (it.type()) {
      case Hash::Type::kSha256:
        if (Hash(Hash::Type::kSha256, Crypto::sha256digestHex(canonical)) != it) {
          // If prefetch is true, it means we're checking a local copy of the metadata.
          // Failures in that case just indicate we need to refresh it from the server, so
          // we only actually log the error if the metadata comes directly from the server.
          if (!prefetch) {
            LOG_ERROR << "Hash verification for " << role << " metadata failed";
          }
          throw Uptane::SecurityException(RepositoryType::IMAGE,
                                          "Snapshot hash mismatch for " + role.ToString() + " metadata");
        }
        break;
      case Hash::Type::kSha512:
        if (Hash(Hash::Type::kSha512, Crypto::sha512digestHex(canonical)) != it) {
          if (!prefetch) {
            LOG_ERROR << "Hash verification for " << role << " metadata failed";
          }
          throw Uptane::SecurityException(RepositoryType::IMAGE,
                                          "Snapshot hash mismatch for " + role.ToString() + " metadata");
        }
        break;
      default:
        break;
    }
  }
}

int ImageRepository::getRoleVersion(const Uptane::Role& role) const { return snapshot.role_version(role); }

int64_t ImageRepository::getRoleSize(const Uptane::Role& role) const { return snapshot.role_size(role); }

void ImageRepository::verifyTargets(const std::string& targets_raw, bool prefetch, bool hash_change_expected) {
  try {
    verifyRoleHashes(targets_raw, Uptane::Role::Targets(), prefetch);

    auto targets_json = Utils::parseJSON(targets_raw);

    // Verify the signature:
    auto signer = std::make_shared<MetaWithKeys>(root);
    targets = std::make_shared<Uptane::Targets>(
        Targets(RepositoryType::Image(), Uptane::Role::Targets(), targets_json, signer));

    if (targets->version() != snapshot.role_version(Uptane::Role::Targets())) {
      throw Uptane::VersionMismatch(RepositoryType::IMAGE, Uptane::Role::TARGETS);
    }
  } catch (const Uptane::SecurityException& e) {
    if (hash_change_expected) {
      LOG_DEBUG << "Signature verification for Image repo Targets metadata failed: " << e.what();
    } else {
      LOG_ERROR << "Signature verification for Image repo Targets metadata failed: " << e.what();
    }
    throw;
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for Image repo Targets metadata failed: " << e.what();
    throw;
  }
}

std::shared_ptr<Uptane::Targets> ImageRepository::verifyDelegation(const std::string& delegation_raw,
                                                                   const Uptane::Role& role,
                                                                   const Targets& parent_target) {
  try {
    const Json::Value delegation_json = Utils::parseJSON(delegation_raw);
    const std::string canonical = Utils::jsonToCanonicalStr(delegation_json);

    // Verify the signature:
    auto signer = std::make_shared<MetaWithKeys>(parent_target);
    return std::make_shared<Uptane::Targets>(Targets(RepositoryType::Image(), role, delegation_json, signer));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for Image repo delegated Targets metadata failed";
    throw;
  }

  return std::shared_ptr<Uptane::Targets>(nullptr);
}

void ImageRepository::checkTargetsExpired() {
  if (targets->isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.ToString(), Role::TARGETS);
  }
}

void ImageRepository::updateRoot(INvStorage& storage, const IMetadataFetcher& fetcher) {
  resetMeta();
  RepositoryCommon::updateRoot(storage, fetcher, RepositoryType::Image());
}

void ImageRepository::updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) {
  const auto timestamp_stored_signature{timestamp.isInitialized() ? timestamp.signature() : ""};
  bool snapshot_updated = false;
  auto prev_timestamp = timestamp;

  updateRoot(storage, fetcher);

  // Update Image repo Timestamp metadata
  {
    std::string image_timestamp;

    fetcher.fetchLatestRole(&image_timestamp, kMaxTimestampSize, RepositoryType::Image(), Role::Timestamp());
    int remote_version = extractVersionUntrusted(image_timestamp);

    int local_version;
    std::string image_timestamp_stored;
    if (storage.loadNonRoot(&image_timestamp_stored, RepositoryType::Image(), Role::Timestamp())) {
      local_version = extractVersionUntrusted(image_timestamp_stored);
    } else {
      local_version = -1;
    }

    verifyTimestamp(image_timestamp);

    if (local_version > remote_version) {
      throw Uptane::SecurityException(RepositoryType::IMAGE, "Rollback attempt");
    } else if (local_version < remote_version || timestamp_stored_signature != timestamp.signature()) {
      // If local and remote versions are the same but their content actually differ then store/update the metadata in
      // DB We assume that the metadata contains just one signature, otherwise the comparison might not always work
      // correctly.
      storage.storeNonRoot(image_timestamp, RepositoryType::Image(), Role::Timestamp());
    }

    checkTimestampExpired();
  }

  // Update Image repo Snapshot metadata
  {
    // First check if we already have the latest version according to the
    // Timestamp metadata.
    bool fetch_snapshot = true;
    int local_version;
    std::string image_snapshot_stored;
    if (storage.loadNonRoot(&image_snapshot_stored, RepositoryType::Image(), Role::Snapshot())) {
      try {
        verifySnapshot(image_snapshot_stored, true);
        fetch_snapshot = false;
        LOG_DEBUG << "Skipping Image repo Snapshot download; stored version is still current.";
      } catch (const Uptane::SecurityException& e) {
        if (!prev_timestamp.isInitialized() || prev_timestamp.snapshot_hashes() != timestamp.snapshot_hashes()) {
          // There were updates in the snapshot hashes within the timestamp meta, so we expect the current snapshot to
          // be invalid, and there is no need to report an error
          LOG_DEBUG << "Image repo Snapshot verification failed: " << e.what();
        } else {
          LOG_ERROR << "Image repo Snapshot verification failed: " << e.what();
        }
      } catch (const Uptane::Exception& e) {
        LOG_INFO << "Downloading new Image repo Snapshot metadata because verification of local copy failed: "
                 << e.what();
      }
      local_version = snapshot.version();
    } else {
      local_version = -1;
    }

    // If we don't, attempt to fetch the latest.
    if (fetch_snapshot) {
      fetchSnapshot(storage, fetcher, local_version);
      snapshot_updated = true;
    }

    checkSnapshotExpired();
  }

  // Update Image repo Targets metadata
  {
    // First check if we already have the latest version according to the
    // Snapshot metadata.
    bool fetch_targets = true;
    int local_version = -1;
    std::string image_targets_stored;
    if (storage.loadNonRoot(&image_targets_stored, RepositoryType::Image(), Role::Targets())) {
      try {
        verifyTargets(image_targets_stored, true, snapshot_updated);
        fetch_targets = false;
        LOG_DEBUG << "Skipping Image repo Targets download; stored version is still current.";
      } catch (const Uptane::SecurityException& e) {
        if (snapshot_updated) {
          LOG_DEBUG << "Image repo Target verification failed: " << e.what();
        } else {
          LOG_ERROR << "Image repo Target verification failed: " << e.what();
        }
      } catch (const std::exception& e) {
        LOG_INFO << "Downloading new Image repo Targets metadata because verification of local copy failed: "
                 << e.what();
      }
      if (targets) {
        local_version = targets->version();
      }
    }

    // If we don't, attempt to fetch the latest.
    if (fetch_targets) {
      fetchTargets(storage, fetcher, local_version);
    }

    checkTargetsExpired();
  }
}

void ImageRepository::checkMetaOffline(INvStorage& storage) {
  resetMeta();
  // Load Image repo Root metadata
  {
    std::string image_root;
    if (!storage.loadLatestRoot(&image_root, RepositoryType::Image())) {
      throw Uptane::SecurityException(RepositoryType::IMAGE, "Could not load latest root");
    }

    initRoot(RepositoryType(RepositoryType::IMAGE), image_root);

    if (rootExpired()) {
      throw Uptane::ExpiredMetadata(RepositoryType::IMAGE, Role::Root().ToString());
    }
  }

  // Load Image repo Timestamp metadata
  {
    std::string image_timestamp;
    if (!storage.loadNonRoot(&image_timestamp, RepositoryType::Image(), Role::Timestamp())) {
      throw Uptane::SecurityException(RepositoryType::IMAGE, "Could not load Timestamp role");
    }

    verifyTimestamp(image_timestamp);

    checkTimestampExpired();
  }

  // Load Image repo Snapshot metadata
  {
    std::string image_snapshot;

    if (!storage.loadNonRoot(&image_snapshot, RepositoryType::Image(), Role::Snapshot())) {
      throw Uptane::SecurityException(RepositoryType::IMAGE, "Could not load Snapshot role");
    }

    verifySnapshot(image_snapshot, false);

    checkSnapshotExpired();
  }

  // Load Image repo Targets metadata
  {
    std::string image_targets;
    Role targets_role = Uptane::Role::Targets();
    if (!storage.loadNonRoot(&image_targets, RepositoryType::Image(), targets_role)) {
      throw Uptane::SecurityException(RepositoryType::IMAGE, "Could not load Image role");
    }

    verifyTargets(image_targets, false, false);

    checkTargetsExpired();
  }
}

}  // namespace Uptane
