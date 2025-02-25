#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <thread>

#include <boost/process.hpp>

#include "libaktualizr/config.h"
#include "libaktualizr/http/httpclient.h"
#include "libaktualizr/image_repo.h"
#include "libaktualizr/logging/logging.h"
#include "libaktualizr/package_manager/ostreemanager.h"
#include "libaktualizr/uptane/fetcher.h"
#include "libaktualizr/uptane/imagerepository.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"

class TufRepoMock {
 public:
  TufRepoMock(const boost::filesystem::path& root_dir, std::string expires = "",
              std::string correlation_id = "corelation-id")
      : repo_{root_dir, expires, correlation_id},
        port_{TestUtils::getFreePort()},
        url_{"http://localhost:" + port_},
        process_{"tests/fake_http_server/fake_test_server.py", port_, "-m", root_dir} {
    repo_.generateRepo(KeyType::kED25519);
    TestUtils::waitForServer(url_ + "/");
  }
  ~TufRepoMock() {
    process_.terminate();
    process_.wait_for(std::chrono::seconds(10));
  }

 public:
  const std::string& url() { return url_; }

  Uptane::Target add_target(const std::string& target_name, const std::string& hash, const std::string& hardware_id) {
    repo_.addCustomImage(target_name, Hash(Hash::Type::kSha256, hash), 0, hardware_id);

    Json::Value target;
    target["length"] = 0;
    target["hashes"]["sha256"] = hash;
    target["custom"]["targetFormat"] = "OSTREE";
    return Uptane::Target(target_name, target);
  }

 private:
  ImageRepo repo_;
  std::string port_;
  std::string url_;
  boost::process::child process_;
};

class Treehub {
 public:
  Treehub(const std::string& root_dir)
      : root_dir_{root_dir},
        port_{TestUtils::getFreePort()},
        url_{"http://localhost:" + port_},
        process_{"tests/sota_tools/treehub_server.py",
                 std::string("-p"),
                 port_,
                 std::string("-d"),
                 root_dir,
                 std::string("-s0.5"),
                 std::string("--create"),
                 std::string("--system")} {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    TestUtils::waitForServer(url_ + "/");
  }

  ~Treehub() {
    process_.terminate();
    process_.wait_for(std::chrono::seconds(10));
  }

  const std::string& url() { return url_; }

  std::string getRev() {
    Process ostree_process{"ostree"};
    Process::Result result = ostree_process.run({"rev-parse", std::string("--repo"), root_dir_, "master"});
    if (std::get<0>(result) != 0) {
      throw std::runtime_error("Failed to get the current ostree revision in Treehub: " + std::get<2>(result));
    }
    auto res_rev = std::get<1>(result);
    boost::trim_if(res_rev, boost::is_any_of(" \t\r\n"));
    return res_rev;
  }

 private:
  const std::string root_dir_;
  std::string port_;
  std::string url_;
  boost::process::child process_;
};

class ComposeAppPackManMock : public OstreeManager {
 public:
  ComposeAppPackManMock(const PackageConfig& pconfig, const BootloaderConfig& bconfig,
                        const std::shared_ptr<INvStorage>& storage, const std::shared_ptr<HttpInterface>& http)
      : OstreeManager(pconfig, bconfig, storage, http) {
    sysroot_ = LoadSysroot(pconfig.sysroot);
  }

  std::string getCurrentHash() const override {
    g_autoptr(GPtrArray) deployments = nullptr;
    OstreeDeployment* deployment = nullptr;

    deployments = ostree_sysroot_get_deployments(sysroot_.get());
    if (deployments != nullptr && deployments->len > 0) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      deployment = static_cast<OstreeDeployment*>(deployments->pdata[0]);
    }
    auto hash = ostree_deployment_get_csum(deployment);
    return hash;
  }

  data::InstallationResult finalizeInstall(const Uptane::Target& target) override {
    auto cur_deployed_hash = getCurrentHash();

    if (target.sha256Hash() == cur_deployed_hash) {
      storage_->saveInstalledVersion("", target, InstalledVersionUpdateMode::kCurrent);
      return data::InstallationResult{data::ResultCode::Numeric::kOk, "Update has been successfully applied"};
    }
    return data::InstallationResult{data::ResultCode::Numeric::kInstallFailed, "Update has failed"};
  }

 private:
  GObjectUniquePtr<OstreeSysroot> sysroot_;
};

class AkliteMock {
 public:
  AkliteMock(const AkliteMock&) = delete;
  AkliteMock(const Config& config)
      : storage_{INvStorage::newStorage(config.storage)},
        key_manager_{std_::make_unique<KeyManager>(storage_, config.keymanagerConfig())},
        http_client_{std::make_shared<HttpClient>()},
        package_manager_{
            std::make_shared<ComposeAppPackManMock>(config.pacman, config.bootloader, storage_, http_client_)},
        fetcher_{std::make_shared<Uptane::Fetcher>(config, http_client_)} {
    finalizeIfNeeded();
  }
  ~AkliteMock() {}

 public:
  data::InstallationResult update() {
    image_repo_.updateMeta(*storage_, *fetcher_);
    image_repo_.checkMetaOffline(*storage_);

    std::shared_ptr<const Uptane::Targets> targets = image_repo_.getTargets();

    if (0 == targets->targets.size()) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    Uptane::Target target{targets->targets[0]};

    if (TargetStatus::kNotFound != package_manager_->verifyTarget(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    if (isTargetCurrent(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    if (!package_manager_->fetchTarget(target, *fetcher_, *key_manager_, nullptr, nullptr)) {
      return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "Failed to download Target");
    }

    if (TargetStatus::kGood != package_manager_->verifyTarget(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, "Target is invalid");
    }

    const auto install_result = package_manager_->install(target);
    if (install_result.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
      storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);
    } else if (install_result.result_code.num_code == data::ResultCode::Numeric::kOk) {
      storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
    }

    return install_result;
  }

  bool isTargetCurrent(const Uptane::Target& target) {
    const auto cur_target = package_manager_->getCurrent();

    if (cur_target.type() != target.type() || target.type() != "OSTREE") {
      LOG_ERROR << "Target formats mismatch: " << cur_target.type() << " != " << target.type();
      return false;
    }

    if (cur_target.length() != target.length()) {
      LOG_INFO << "Target names differ " << cur_target.filename() << " != " << target.filename();
      return false;
    }

    if (cur_target.filename() != target.filename()) {
      LOG_INFO << "Target names differ " << cur_target.filename() << " != " << target.filename();
      return false;
    }

    if (cur_target.sha256Hash() != target.sha256Hash()) {
      LOG_ERROR << "Target hashes differ " << cur_target.sha256Hash() << " != " << target.sha256Hash();
      return false;
    }

    return true;
  }

  data::InstallationResult finalizeIfNeeded() {
    boost::optional<Uptane::Target> pending_version;
    storage_->loadInstalledVersions("", nullptr, &pending_version);
    if (!!pending_version) {
      return package_manager_->finalizeInstall(*pending_version);
    }
    return data::InstallationResult{data::ResultCode::Numeric::kAlreadyProcessed, "Already installed"};
  }

 private:
  std::shared_ptr<INvStorage> storage_;
  std::unique_ptr<KeyManager> key_manager_;
  std::shared_ptr<HttpClient> http_client_;
  std::shared_ptr<PackageManagerInterface> package_manager_;
  std::shared_ptr<Uptane::Fetcher> fetcher_;
  Uptane::ImageRepository image_repo_;

  FRIEND_TEST(AkliteTest, hashMismatchLogsTest);
};

class AkliteTest : public ::testing::Test {
 public:
  static std::string SysRootSrc;

 protected:
  AkliteTest()
      : tuf_repo_{test_dir_.Path() / "repo"},
        treehub_{(test_dir_.Path() / "treehub").string()},
        sysroot_path_{test_dir_.Path() / "sysroot"} {
    const auto sysroot_cmd = std::string("cp -r ") + SysRootSrc + std::string(" ") + sysroot_path_.string();
    if (0 != system(sysroot_cmd.c_str())) {
      throw std::runtime_error("Failed to copy a system rootfs");
    }

    conf_.uptane.repo_server = tufRepo().url() + "/repo";
    conf_.provision.primary_ecu_hardware_id = "primary_hw";
    conf_.pacman.type = PACKAGE_MANAGER_OSTREE;
    conf_.pacman.sysroot = sysrootPath();
    conf_.pacman.ostree_server = treehub().url();
    conf_.pacman.os = "dummy-os";
    conf_.storage.path = test_dir_.Path();
  }

  TufRepoMock& tufRepo() { return tuf_repo_; }
  Treehub& treehub() { return treehub_; }
  boost::filesystem::path& sysrootPath() { return sysroot_path_; }
  Config& conf() { return conf_; }

  static void corruptStoredMetadata(std::shared_ptr<INvStorage> storage, const Uptane::Role& role) {
    std::string metadata_stored;
    EXPECT_TRUE(storage->loadNonRoot(&metadata_stored, Uptane::RepositoryType::Image(), role));
    logger_set_threshold(boost::log::trivial::error);
    EXPECT_EQ('{', metadata_stored[0]);
    metadata_stored[0] = '[';
    storage->storeNonRoot(metadata_stored, Uptane::RepositoryType::Image(), role);
  }

 private:
  TemporaryDirectory test_dir_;
  TufRepoMock tuf_repo_;
  Treehub treehub_;
  boost::filesystem::path sysroot_path_;
  Config conf_;
};

std::string AkliteTest::SysRootSrc;

/*
 * Test that mimics aktualizr-lite
 *
 * It makes use of libaktualizr's components and makes API calls to them in the way as aktualizr-lite
 * would do during its regular update cycle.
 */
TEST_F(AkliteTest, ostreeUpdate) {
  const auto target_to_install = tufRepo().add_target("target-01", treehub().getRev(), "primary_hw");
  {
    AkliteMock aklite{conf()};
    const auto update_result = aklite.update();
    ASSERT_EQ(update_result.result_code.num_code, data::ResultCode::Numeric::kNeedCompletion);
  }
  // reboot emulation by destroying and creating of a new AkliteMock instance
  {
    AkliteMock aklite{conf()};
    ASSERT_TRUE(aklite.isTargetCurrent(target_to_install));
  }
}

/*
 * Test that verifies if metadata files are being stored when there are changes and also not being stored
 * when the files have not changed.
 */
TEST_F(AkliteTest, timestampStoreLogsTest) {
  AkliteMock aklite{conf()};
  std::string log_output;
  tufRepo().add_target("target-01", treehub().getRev(), "primary_hw");

  // On first update, metadata is expected to be stored
  testing::internal::CaptureStdout();
  logger_set_threshold(boost::log::trivial::debug);
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_NE(std::string::npos, log_output.find("Storing timestamp for image repo"));
  EXPECT_NE(std::string::npos, log_output.find("Storing snapshot for image repo"));
  EXPECT_NE(std::string::npos, log_output.find("Storing targets for image repo"));

  // If there were no changes, no metadata should be stored
  testing::internal::CaptureStdout();
  logger_set_threshold(boost::log::trivial::debug);
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(std::string::npos, log_output.find("Storing timestamp for image repo"));
  EXPECT_EQ(std::string::npos, log_output.find("Storing snapshot for image repo"));
  EXPECT_EQ(std::string::npos, log_output.find("Storing targets for image repo"));
}

/*
 * Test that verifies if hash verification failures for targets and snapshot have the expected severity:
 *  - Debug when a hash mismatch is expected, e.g., when snapshot hashes change inside the timestamp metadata;
 *  - Error when the hash mismatch is not expected
 */
TEST_F(AkliteTest, hashMismatchLogsTest) {
  AkliteMock aklite{conf()};
  std::string log_output;
  tufRepo().add_target("target-01", treehub().getRev(), "primary_hw");
  aklite.update();

  // First, verify error output
  logger_set_threshold(boost::log::trivial::error);

  // Corrupt stored targets metadata and verify that the expected errors messages are generated
  corruptStoredMetadata(aklite.storage_, Uptane::Role::Targets());
  testing::internal::CaptureStdout();
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_NE(std::string::npos, log_output.find("Signature verification for Image repo Targets metadata failed: "
                                               "Snapshot hash mismatch for targets metadata"));
  EXPECT_NE(std::string::npos,
            log_output.find("Image repo Target verification failed: Snapshot hash mismatch for targets metadata"));

  // Corrupt stored snapshot metadata and verify that the expected error message is generated
  corruptStoredMetadata(aklite.storage_, Uptane::Role::Snapshot());
  testing::internal::CaptureStdout();
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_NE(std::string::npos,
            log_output.find("Image repo Snapshot verification failed: Snapshot metadata hash verification failed"));

  // No metadata corruption: no errors should be generated
  testing::internal::CaptureStdout();
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(log_output.empty());

  // Targets updated, no metadata corruption: no errors should be generated
  tufRepo().add_target("target-02", treehub().getRev(), "primary_hw");
  testing::internal::CaptureStdout();
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(log_output.empty());

  // Verify debug output
  logger_set_threshold(boost::log::trivial::debug);

  // Targets updated, no metadata corruption: verify expected debug messages
  tufRepo().add_target("target-03", treehub().getRev(), "primary_hw");
  testing::internal::CaptureStdout();
  aklite.update();
  log_output = testing::internal::GetCapturedStdout();
  EXPECT_NE(std::string::npos, log_output.find("Signature verification for Image repo Targets metadata failed: "
                                               "Snapshot hash mismatch for targets metadata"));
  EXPECT_NE(std::string::npos,
            log_output.find("Image repo Target verification failed: Snapshot hash mismatch for targets metadata"));
  EXPECT_NE(std::string::npos,
            log_output.find("Image repo Snapshot verification failed: Snapshot metadata hash verification failed"));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the test OSTree sysroot\n";
    return EXIT_FAILURE;
  }
  AkliteTest::SysRootSrc = argv[1];

  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
