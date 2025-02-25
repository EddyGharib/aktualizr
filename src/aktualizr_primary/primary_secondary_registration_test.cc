#include <gtest/gtest.h>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include "metafake.h"
#include "secondary.h"
#include "uptane_test_common.h"
#include "libaktualizr/utilities/utils.h"

boost::filesystem::path fake_meta_dir;

/* This tests that a device that had an IP Secondary will still find it after
 * recent changes, even if it does not connect when the device starts. Note that
 * this is only supported for a single IP Secondary. */
TEST(PrimarySecondaryReg, SecondariesMigration) {
  const Uptane::EcuSerial primary_serial{"p_serial"};
  const Uptane::EcuSerial secondary_serial{"s_serial"};
  const Uptane::HardwareIdentifier primary_hwid{"p_hwid"};
  const Uptane::HardwareIdentifier secondary_hwid{"s_hwid"};

  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates", fake_meta_dir);
  const auto& url = http->tls_server;

  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = url + "/director";
  conf.uptane.repo_server = url + "/repo";
  conf.provision.server = url;
  conf.provision.primary_ecu_serial = primary_serial.ToString();
  conf.provision.primary_ecu_hardware_id = primary_hwid.ToString();
  conf.storage.path = temp_dir.Path();
  conf.import.base_path = temp_dir.Path() / "import";
  conf.tls.server = url;
  conf.bootloader.reboot_sentinel_dir = temp_dir.Path();
  const boost::filesystem::path sec_conf_path = temp_dir / "s_config.json";
  conf.uptane.secondary_config_file = sec_conf_path;

  auto storage = INvStorage::newStorage(conf.storage);
  Json::Value sec_conf;

  // Prepare storage the "old" way (without the secondary_ecus table):
  storage->storeDeviceId("device");
  storage->storeEcuSerials({{primary_serial, primary_hwid}, {secondary_serial, secondary_hwid}});
  storage->storeEcuRegistered();

  // Also test backwards compatibility with verification_type here. Don't set
  // it but expect it to be set to Full.
  sec_conf["IP"]["secondary_wait_port"] = 9030;
  sec_conf["IP"]["secondary_wait_timeout"] = 1;
  sec_conf["IP"]["secondaries"] = Json::arrayValue;
  sec_conf["IP"]["secondaries"][0]["addr"] = "127.0.0.1:9061";
  Utils::writeFile(sec_conf_path, sec_conf);

  {
    // Confirm that the fields from the secondary_ecus table are empty.
    std::vector<SecondaryInfo> secs_info;
    storage->loadSecondariesInfo(&secs_info);
    EXPECT_EQ(secs_info.size(), 1);
    EXPECT_EQ(secs_info[0].serial.ToString(), secondary_serial.ToString());
    EXPECT_EQ(secs_info[0].type, "");
    EXPECT_EQ(secs_info[0].extra, "");
  }

  {
    // Verify that aktualizr can still start if it can't connect to its
    // Secondary. This will migrate the Secondary.
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::initSecondaries(aktualizr, sec_conf_path);
    aktualizr.Initialize();
    aktualizr.CheckUpdates().get();

    std::vector<SecondaryInfo> secs_info;
    storage->loadSecondariesInfo(&secs_info);
    EXPECT_EQ(secs_info.size(), 1);
    EXPECT_EQ(secs_info[0].serial.ToString(), secondary_serial.ToString());
    EXPECT_EQ(secs_info[0].type, "IP");
    EXPECT_EQ(secs_info[0].extra, R"({"ip":"127.0.0.1","port":9061,"verification_type":"Full"})");
  }

  {
    // Try again (again without connecting) to verify that the Secondary is
    // correctly found in the storage.
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::initSecondaries(aktualizr, sec_conf_path);
    aktualizr.Initialize();
    aktualizr.CheckUpdates().get();

    std::vector<SecondaryInfo> secs_info;
    storage->loadSecondariesInfo(&secs_info);
    EXPECT_EQ(secs_info.size(), 1);
    EXPECT_EQ(secs_info[0].serial.ToString(), secondary_serial.ToString());
    EXPECT_EQ(secs_info[0].type, "IP");
    EXPECT_EQ(secs_info[0].extra, R"({"ip":"127.0.0.1","port":9061,"verification_type":"Full"})");
  }
}

/*
 * Register Virtual Secondaries via json configuration.
 * Reject multiple Secondaries with the same serial.
 */
TEST(PrimarySecondaryReg, VirtualSecondary) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);

  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  // This should fail because TestAktualizr automatically adds the default
  // Secondary created in makeTestConfig.
  EXPECT_THROW(Primary::initSecondaries(aktualizr, conf.uptane.secondary_config_file), std::exception);

  boost::filesystem::remove(conf.uptane.secondary_config_file);
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "serial2", "hwid2");
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "serial3", "hwid3");
  Primary::initSecondaries(aktualizr, conf.uptane.secondary_config_file);
  aktualizr.Initialize();

  std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "secondary_ecu_serial", "serial2", "serial3"};
  UptaneTestCommon::verifyEcus(temp_dir, expected_ecus);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  TemporaryDirectory tmp_dir;
  fake_meta_dir = tmp_dir.Path();
  CreateFakeRepoMetaData(fake_meta_dir);

  return RUN_ALL_TESTS();
}
#endif
