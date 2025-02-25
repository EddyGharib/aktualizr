#include <fstream>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "libaktualizr/crypto/crypto.h"
#include "libaktualizr/utilities/utils.h"
#include "utilities/fault_injection.h"
#include "virtualsecondary.h"

namespace Primary {

constexpr const char* const VirtualSecondaryConfig::Type;

VirtualSecondaryConfig::VirtualSecondaryConfig(const Json::Value& json_config) : ManagedSecondaryConfig(Type) {
  partial_verifying = json_config["partial_verifying"].asBool();
  ecu_serial = json_config["ecu_serial"].asString();
  ecu_hardware_id = json_config["ecu_hardware_id"].asString();
  full_client_dir = json_config["full_client_dir"].asString();
  ecu_private_key = json_config["ecu_private_key"].asString();
  ecu_public_key = json_config["ecu_public_key"].asString();
  firmware_path = json_config["firmware_path"].asString();
  target_name_path = json_config["target_name_path"].asString();
  metadata_path = json_config["metadata_path"].asString();
}

std::vector<VirtualSecondaryConfig> VirtualSecondaryConfig::create_from_file(
    const boost::filesystem::path& file_full_path) {
  Json::Value json_config;
  std::ifstream json_file(file_full_path.string());
  Json::parseFromStream(Json::CharReaderBuilder(), json_file, &json_config, nullptr);
  json_file.close();

  std::vector<VirtualSecondaryConfig> sec_configs;
  sec_configs.reserve(json_config[Type].size());

  for (const auto& item : json_config[Type]) {
    sec_configs.emplace_back(VirtualSecondaryConfig(item));
  }
  return sec_configs;
}

void VirtualSecondaryConfig::dump(const boost::filesystem::path& file_full_path) const {
  Json::Value json_config;

  json_config["partial_verifying"] = partial_verifying;
  json_config["ecu_serial"] = ecu_serial;
  json_config["ecu_hardware_id"] = ecu_hardware_id;
  json_config["full_client_dir"] = full_client_dir.string();
  json_config["ecu_private_key"] = ecu_private_key;
  json_config["ecu_public_key"] = ecu_public_key;
  json_config["firmware_path"] = firmware_path.string();
  json_config["target_name_path"] = target_name_path.string();
  json_config["metadata_path"] = metadata_path.string();

  Json::Value root;
  // Append to the config file if it already exists.
  if (boost::filesystem::exists(file_full_path)) {
    root = Utils::parseJSONFile(file_full_path);
  }
  root[Type].append(json_config);

  Json::StreamWriterBuilder json_bwriter;
  json_bwriter["indentation"] = "\t";
  std::unique_ptr<Json::StreamWriter> const json_writer(json_bwriter.newStreamWriter());

  boost::filesystem::create_directories(file_full_path.parent_path());
  std::ofstream json_file(file_full_path.string());
  json_writer->write(root, &json_file);
  json_file.close();
}

VirtualSecondary::VirtualSecondary(Primary::VirtualSecondaryConfig sconfig_in)
    : ManagedSecondary(std::move(sconfig_in)) {}

data::InstallationResult VirtualSecondary::putMetadata(const Uptane::Target& target) {
  if (fiu_fail("secondary_putmetadata") != 0) {
    return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, fault_injection_last_info());
  }

  return ManagedSecondary::putMetadata(target);
}

data::InstallationResult VirtualSecondary::putRoot(const std::string& root, bool director) {
  if (fiu_fail("secondary_putroot") != 0) {
    return data::InstallationResult(data::ResultCode(data::ResultCode::Numeric::kOk, fault_injection_last_info()),
                                    "Forced failure");
  }

  return ManagedSecondary::putRoot(root, director);
}

data::InstallationResult VirtualSecondary::sendFirmware(const Uptane::Target& target) {
  if (fiu_fail((std::string("secondary_sendfirmware_") + getSerial().ToString()).c_str()) != 0) {
    // Put the injected failure string into the ResultCode so that it shows up
    // in the device's concatenated InstallationResult.
    return data::InstallationResult(
        data::ResultCode(data::ResultCode::Numeric::kDownloadFailed, fault_injection_last_info()), "Forced failure");
  }

  return ManagedSecondary::sendFirmware(target);
}

data::InstallationResult VirtualSecondary::install(const Uptane::Target& target) {
  if (fiu_fail((std::string("secondary_install_") + getSerial().ToString()).c_str()) != 0) {
    // Put the injected failure string into the ResultCode so that it shows up
    // in the device's concatenated InstallationResult.
    return data::InstallationResult(
        data::ResultCode(data::ResultCode::Numeric::kInstallFailed, fault_injection_last_info()), "Forced failure");
  }

  return ManagedSecondary::install(target);
}

}  // namespace Primary
