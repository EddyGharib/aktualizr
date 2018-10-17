#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "fetcher.h"
#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "tuf.h"

static std::string server = "http://127.0.0.1:";

TEST(fetcher, fetch_with_pause) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "d03b1a2081755f3a5429854cc3e700f8cbf125db2bd77098ae79a7d783256a7d";
  target_json["length"] = 2048;

  Uptane::Target target("large_file", target_json);
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.uptane.repo_server = server;

  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  Uptane::Fetcher f(config, storage, http);

  std::thread([&f] {
    f.setPause(true);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    f.setPause(false);
  })
      .detach();

  auto start = std::chrono::high_resolution_clock::now();
  auto result = f.fetchVerifyTarget(target);
  auto finish = std::chrono::high_resolution_clock::now();

  EXPECT_TRUE(result);
  EXPECT_GE((finish - start), std::chrono::seconds(2));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::string port = TestUtils::getFreePort();
  server += port;
  TestHelperProcess server_process("tests/fake_http_server/fake_http_server.py", port);

  sleep(3);

  return RUN_ALL_TESTS();
}
#endif
