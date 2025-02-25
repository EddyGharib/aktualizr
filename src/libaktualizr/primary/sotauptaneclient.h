#ifndef SOTA_UPTANE_CLIENT_H_
#define SOTA_UPTANE_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/signals2.hpp>
#include "gtest/gtest_prod.h"
#include "json/json.h"

#include "libaktualizr/campaign.h"
#include "libaktualizr/config.h"
#include "libaktualizr/events.h"
#include "libaktualizr/packagemanagerfactory.h"
#include "libaktualizr/packagemanagerinterface.h"
#include "libaktualizr/results.h"
#include "libaktualizr/secondaryinterface.h"

#include "libaktualizr/bootloader/bootloader.h"
#include "libaktualizr/http/httpclient.h"
#include "libaktualizr/primary/reportqueue.h"
#include "libaktualizr/uptane/exceptions.h"
#include "libaktualizr/uptane/fetcher.h"
#include "libaktualizr/uptane/imagerepository.h"
#include "libaktualizr/uptane/tuf.h"
#include "libaktualizr/utilities/apiqueue.h"
#include "primary/secondary_provider_builder.h"
#include "provisioner.h"
#include "uptane/directorrepository.h"
#include "uptane/iterator.h"
#include "uptane/manifest.h"

class SotaUptaneClient {
 public:
  /**
   * Provisioning was needed, attempted and failed.
   * Thrown by requiresProvision().
   */
  class ProvisioningFailed : public std::runtime_error {
   public:
    explicit ProvisioningFailed() : std::runtime_error("Device was not able provision on-line") {}
  };

  /**
   * Device must be provisioned before calling this operation.
   * Thrown by requiresAlreadyProvisioned().
   */
  class NotProvisionedYet : public std::runtime_error {
   public:
    explicit NotProvisionedYet() : std::runtime_error("Device is not provisioned on-line yet") {}
  };

  SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in,
                   std::shared_ptr<event::Channel> events_channel_in);

  SotaUptaneClient(Config &config_in, const std::shared_ptr<INvStorage> &storage_in)
      : SotaUptaneClient(config_in, storage_in, std::make_shared<HttpClient>(), nullptr) {}

  void initialize();
  void addSecondary(const std::shared_ptr<SecondaryInterface> &sec);

  /**
   * Make one attempt at provisioning on-line.
   * If the device is already provisioned then this is a no-op.
   * @return True if the device has completed on-line provisioning
   */
  bool attemptProvision();

  result::Download downloadImages(const std::vector<Uptane::Target> &targets,
                                  const api::FlowControlToken *token = nullptr);

  /** See Aktualizr::SetCustomHardwareInfo(Json::Value) */
  void setCustomHardwareInfo(Json::Value hwinfo) { custom_hardware_info_ = std::move(hwinfo); }
  void reportPause();
  void reportResume();
  void sendDeviceData();
  result::UpdateCheck fetchMeta();
  bool putManifest(const Json::Value &custom = Json::nullValue);
  result::Install uptaneInstall(const std::vector<Uptane::Target> &updates);
  result::CampaignCheck campaignCheck();
  void campaignAccept(const std::string &campaign_id);
  void campaignDecline(const std::string &campaign_id);
  void campaignPostpone(const std::string &campaign_id);
  bool hasPendingUpdates() const;
  bool isInstallCompletionRequired();
  void completeInstall();
  std::vector<Uptane::Target> getStoredTargets() const { return package_manager_->getTargetFiles(); }
  void deleteStoredTarget(const Uptane::Target &target) { package_manager_->removeTargetFile(target); }
  std::ifstream openStoredTarget(const Uptane::Target &target);

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, DeviceInstallationResult);
  FRIEND_TEST(Aktualizr, DeviceInstallationResultMetadata);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckNoUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Aktualizr, FinalizationFailure);
  FRIEND_TEST(Aktualizr, InstallationFailure);
  FRIEND_TEST(Aktualizr, AutoRebootAfterUpdate);
  FRIEND_TEST(Aktualizr, EmptyTargets);
  FRIEND_TEST(Aktualizr, FullOstreeUpdate);
  FRIEND_TEST(Aktualizr, DownloadNonOstreeBin);
  FRIEND_TEST(Uptane, AssembleManifestGood);
  FRIEND_TEST(Uptane, AssembleManifestBad);
  FRIEND_TEST(Uptane, InstallFakeGood);
  FRIEND_TEST(Uptane, restoreVerify);
  FRIEND_TEST(Uptane, PutManifest);
  FRIEND_TEST(Uptane, offlineIteration);
  FRIEND_TEST(Uptane, IgnoreUnknownUpdate);
  FRIEND_TEST(Uptane, kRejectAllTest);
  FRIEND_TEST(UptaneCI, ProvisionAndPutManifest);
  FRIEND_TEST(UptaneCI, CheckKeys);
  FRIEND_TEST(UptaneKey, Check);  // Note hacky name
  FRIEND_TEST(UptaneNetwork, DownloadFailure);
  FRIEND_TEST(UptaneNetwork, LogConnectivityRestored);
  FRIEND_TEST(UptaneOstree, InitialManifest);
  FRIEND_TEST(UptaneVector, Test);
  FRIEND_TEST(aktualizr_secondary_uptane, credentialsPassing);
  FRIEND_TEST(MetadataExpirationTest, MetadataExpirationAfterInstallationAndBeforeApplication);
  FRIEND_TEST(MetadataExpirationTest, MetadataExpirationAfterInstallationAndBeforeReboot);
  FRIEND_TEST(MetadataExpirationTest, MetadataExpirationBeforeInstallation);
  FRIEND_TEST(Delegation, IterateAll);

  /**
   * This operation requires that the device is provisioned.
   * Make one attempt at provisioning on-line, and if it fails throw a
   * ProvisioningFailed exception.
   */
  void requiresProvision();

  /**
   * This operation requires that the device is already provisioned.
   * If it isn't then immediately throw a NotProvisionedYet exception without
   * attempting any network communications.
   */
  void requiresAlreadyProvisioned();

  data::InstallationResult PackageInstall(const Uptane::Target &target);
  std::pair<bool, Uptane::Target> downloadImage(const Uptane::Target &target,
                                                const api::FlowControlToken *token = nullptr);
  void uptaneIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  void uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  result::UpdateCheck checkUpdates();
  result::UpdateStatus checkUpdatesOffline(const std::vector<Uptane::Target> &targets);
  Json::Value AssembleManifest();
  std::exception_ptr getLastException() const { return last_exception; }
  Uptane::Target getCurrent() const { return package_manager_->getCurrent(); }

  static std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets,
                                                const Uptane::EcuSerial &ecu_id);
  data::InstallationResult PackageInstallSetResult(const Uptane::Target &target);
  void finalizeAfterReboot();
  // Part of sendDeviceData()
  void reportHwInfo();
  // Part of sendDeviceData()
  void reportInstalledPackages();
  // Called by sendDeviceData() and fetchMeta()
  void reportNetworkInfo();
  // Part of sendDeviceData()
  void reportAktualizrConfiguration();
  bool waitSecondariesReachable(const std::vector<Uptane::Target> &updates);
  void storeInstallationFailure(const data::InstallationResult &result);
  data::InstallationResult rotateSecondaryRoot(Uptane::RepositoryType repo, SecondaryInterface &secondary);
  void sendMetadataToEcus(const std::vector<Uptane::Target> &targets, data::InstallationResult *result,
                          std::string *raw_installation_report);
  std::future<data::InstallationResult> sendFirmwareAsync(SecondaryInterface &secondary, const Uptane::Target &target);
  std::vector<result::Install::EcuReport> sendImagesToEcus(const std::vector<Uptane::Target> &targets);

  bool putManifestSimple(const Json::Value &custom = Json::nullValue);
  void getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count = nullptr);
  void updateDirectorMeta();
  void updateImageMeta();
  void checkDirectorMetaOffline();
  void checkImageMetaOffline();

  void computeDeviceInstallationResult(data::InstallationResult *result, std::string *raw_installation_report);
  std::unique_ptr<Uptane::Target> findTargetInDelegationTree(const Uptane::Target &target, bool offline);
  std::unique_ptr<Uptane::Target> findTargetHelper(const Uptane::Targets &cur_targets,
                                                   const Uptane::Target &queried_target, int level, bool terminating,
                                                   bool offline);
  Uptane::LazyTargetsList allTargets() const;
  void checkAndUpdatePendingSecondaries();
  Uptane::EcuSerial primaryEcuSerial() { return provisioner_.PrimaryEcuSerial(); }
  boost::optional<Uptane::HardwareIdentifier> getEcuHwId(const Uptane::EcuSerial &serial);

  template <class T, class... Args>
  void sendEvent(Args &&...args) {
    std::shared_ptr<event::BaseEvent> event = std::make_shared<T>(std::forward<Args>(args)...);
    if (event) {
      if (events_channel) {
        (*events_channel)(std::move(event));
      } else if (!event->isTypeOf<event::DownloadProgressReport>()) {
        LOG_INFO << "got " << event->variant << " event";
      }
    }
  }

  Config &config;
  Uptane::DirectorRepository director_repo;
  Uptane::ImageRepository image_repo;
  Uptane::ManifestIssuer::Ptr uptane_manifest;
  std::shared_ptr<INvStorage> storage;
  std::shared_ptr<HttpInterface> http;
  std::shared_ptr<PackageManagerInterface> package_manager_;
  std::shared_ptr<KeyManager> key_manager_;
  std::shared_ptr<Uptane::Fetcher> uptane_fetcher;
  std::unique_ptr<ReportQueue> report_queue;
  std::shared_ptr<SecondaryProvider> secondary_provider_;
  std::shared_ptr<event::Channel> events_channel;
  std::exception_ptr last_exception;
  // ecu_serial => secondary*
  std::map<Uptane::EcuSerial, SecondaryInterface::Ptr> secondaries;
  std::mutex download_mutex;
  Provisioner provisioner_;
  Json::Value custom_hardware_info_{Json::nullValue};
};

#endif  // SOTA_UPTANE_CLIENT_H_
