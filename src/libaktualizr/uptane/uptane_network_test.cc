/**
 * \file
 *
 * Check that aktualizr can complete provisioning after encountering various
 * network issues.
 */
#include <gtest/gtest.h>
#include <gtest/internal/gtest-port.h>

#include <boost/process.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "httpfake.h"
#include "libaktualizr/http/httpclient.h"
#include "libaktualizr/logging/logging.h"
#include "libaktualizr/storage/invstorage.h"
#include "libaktualizr/utilities/utils.h"
#include "primary/provisioner.h"
#include "primary/sotauptaneclient.h"
#include "test_utils.h"
#include "libaktualizr/uptane/uptanerepository.h"
#include "uptane_test_common.h"

Config conf("tests/config/basic.toml");
std::string port;

bool doTestInit(const std::string &device_register_state, const std::string &ecu_register_state) {
  LOG_INFO << "First attempt to initialize.";
  TemporaryDirectory temp_dir;
  conf.storage.type = StorageType::kSqlite;
  conf.storage.path = temp_dir.Path();
  conf.provision.expiry_days = device_register_state;
  conf.provision.primary_ecu_serial = ecu_register_state;
  const std::string good_url = conf.provision.server;
  if (device_register_state == "noconnection") {
    conf.provision.server = conf.provision.server.substr(conf.provision.server.size() - 2) + "11";
  }

  bool result;
  auto http = std::make_shared<HttpClient>();
  http->timeout(1000);
  auto store = INvStorage::newStorage(conf.storage);
  {
    auto keys = std::make_shared<KeyManager>(store, conf.keymanagerConfig());
    Provisioner provisioner(conf.provision, store, http, keys, {});
    result = provisioner.Attempt();
  }
  if (device_register_state != "noerrors" || ecu_register_state != "noerrors") {
    EXPECT_FALSE(result);
    LOG_INFO << "Second attempt to initialize.";
    conf.provision.server = good_url;
    conf.provision.expiry_days = "noerrors";
    conf.provision.primary_ecu_serial = "noerrors";

    auto keys = std::make_shared<KeyManager>(store, conf.keymanagerConfig());
    Provisioner provisioner(conf.provision, store, http, keys, {});
    result = provisioner.Attempt();
  }

  return result;
}

TEST(UptaneNetwork, DeviceDropRequest) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("drop_request", "noerrors"));
}

TEST(UptaneNetwork, DeviceDropBody) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("drop_body", "noerrors"));
}

TEST(UptaneNetwork, Device503) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("status_503", "noerrors"));
}

TEST(UptaneNetwork, Device408) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("status_408", "noerrors"));
}

TEST(UptaneNetwork, EcuDropRequest) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("noerrors", "drop_request"));
}

TEST(UptaneNetwork, Ecu503) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("noerrors", "status_503"));
}

TEST(UptaneNetwork, Ecu408) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("noerrors", "status_408"));
}

TEST(UptaneNetwork, NoConnection) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");

  EXPECT_TRUE(doTestInit("noconnection", "noerrors"));
}

TEST(UptaneNetwork, NoErrors) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit("noerrors", "noerrors"));
}

TEST(UptaneNetwork, DownloadFailure) {
  TemporaryDirectory temp_dir;
  conf.storage.path = temp_dir.Path();
  conf.pacman.images_path = temp_dir.Path() / "images";
  conf.provision.expiry_days = "download_failure";
  conf.provision.primary_ecu_serial = "download_failure";
  conf.provision.primary_ecu_hardware_id = "hardware_id";

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = std_::make_unique<SotaUptaneClient>(conf, storage);
  EXPECT_NO_THROW(up->initialize());

  Json::Value ot_json;
  ot_json["custom"]["ecuIdentifiers"][conf.provision.primary_ecu_serial]["hardwareId"] =
      conf.provision.primary_ecu_hardware_id;
  ot_json["custom"]["targetFormat"] = "binary";
  ot_json["length"] = 2048;
  ot_json["hashes"]["sha256"] = "d03b1a2081755f3a5429854cc3e700f8cbf125db2bd77098ae79a7d783256a7d";
  Uptane::Target package_to_install{conf.provision.primary_ecu_serial, ot_json};

  std::pair<bool, Uptane::Target> result = up->downloadImage(package_to_install);
  EXPECT_TRUE(result.first);
}

/*
 * Output a log when connectivity is restored.
 */
class HttpUnstable : public HttpFake {
 public:
  explicit HttpUnstable(const boost::filesystem::path &test_dir_in) : HttpFake(test_dir_in, "hasupdates") {}
  HttpResponse get(const std::string &url, int64_t maxsize) override {
    if (!connectSwitch) {
      return HttpResponse({}, 503, CURLE_OK, "");
    } else {
      return HttpFake::get(url, maxsize);
    }
  }

  HttpResponse put(const std::string &url, const Json::Value &data) override {
    if (!connectSwitch) {
      (void)data;
      return HttpResponse(url, 503, CURLE_OK, "");
    } else {
      return HttpFake::put(url, data);
    }
  }

  HttpResponse post(const std::string &url, const Json::Value &data) override {
    if (!connectSwitch) {
      (void)data;
      return HttpResponse(url, 503, CURLE_OK, "");
    } else {
      return HttpFake::post(url, data);
    }
  }

  bool connectSwitch = true;
};

TEST(UptaneNetwork, LogConnectivityRestored) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpUnstable>(temp_dir.Path());
  Config config = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  config.storage.path = temp_dir.Path();
  config.tls.server = http->tls_server;

  auto storage = INvStorage::newStorage(config.storage);
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(up->initialize());

  result::UpdateCheck result = up->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kUpdatesAvailable);

  http->connectSwitch = false;
  result = up->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);

  http->connectSwitch = true;
  testing::internal::CaptureStdout();
  result = up->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kUpdatesAvailable);
  EXPECT_NE(std::string::npos, testing::internal::GetCapturedStdout().find("Connectivity is restored."));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);

  port = TestUtils::getFreePort();
  boost::process::child server_process("tests/fake_http_server/fake_uptane_server.py", port);
  TestUtils::waitForServer("http://127.0.0.1:" + port + "/");

  conf.provision.server = "http://127.0.0.1:" + port;
  conf.tls.server = "http://127.0.0.1:" + port;
  conf.provision.ecu_registration_endpoint = conf.tls.server + "/director/ecus";
  conf.uptane.repo_server = conf.tls.server + "/repo";
  conf.uptane.director_server = conf.tls.server + "/director";
  conf.pacman.ostree_server = conf.tls.server + "/treehub";

  return RUN_ALL_TESTS();
}
#endif
