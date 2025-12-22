#include "libaktualizr/crypto/p11engine.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/provider.h>
#include <openssl/store.h>

#include <libp11.h>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/scoped_array.hpp>

#include "libaktualizr/crypto/crypto.h"
#include "libaktualizr/utilities/utils.h"
#include "utilities/config_utils.h"

P11Engine* P11EngineGuard::instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
int P11EngineGuard::ref_counter = 0;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {

std::string opensslErrOnce() {
  unsigned long e = ERR_get_error();  // NOLINT
  if (e == 0) return {};
  char buf[256];
  ERR_error_string_n(e, buf, sizeof(buf));
  return std::string(buf);
}

struct ProviderBundle {
  OSSL_PROVIDER* deflt{nullptr};
  OSSL_PROVIDER* legacy{nullptr};
  OSSL_PROVIDER* pkcs11{nullptr};

  void load() {
    deflt = OSSL_PROVIDER_load(nullptr, "default");
    if (!deflt) {
      throw std::runtime_error("OpenSSL: failed to load default provider: " + opensslErrOnce());
    }
    legacy = OSSL_PROVIDER_load(nullptr, "legacy");

    pkcs11 = OSSL_PROVIDER_load(nullptr, "pkcs11");
    if (!pkcs11) {
      throw std::runtime_error(
          "OpenSSL: failed to load pkcs11 provider. Ensure a PKCS#11 provider is installed and discoverable. Error: " +
          opensslErrOnce());
    }
  }

  ~ProviderBundle() {
    if (pkcs11) OSSL_PROVIDER_unload(pkcs11);
    if (legacy) OSSL_PROVIDER_unload(legacy);
    if (deflt) OSSL_PROVIDER_unload(deflt);
  }
};

ProviderBundle& providers() {
  static ProviderBundle bundle;
  static bool loaded = false;
  if (!loaded) {
    bundle.load();
    loaded = true;
  }
  return bundle;
}

EVP_PKEY* loadPublicKeyFromPkcs11Uri(const std::string& uri) {
  OSSL_STORE_CTX* store = OSSL_STORE_open(uri.c_str(), nullptr, nullptr, nullptr, nullptr);
  if (!store) {
    throw std::runtime_error("OSSL_STORE_open failed for URI: " + uri + " error: " + opensslErrOnce());
  }

  EVP_PKEY* pkey = nullptr;

  while (!OSSL_STORE_eof(store)) {
    OSSL_STORE_INFO* info = OSSL_STORE_load(store);
    if (!info) {
      const std::string err = opensslErrOnce();
      OSSL_STORE_close(store);
      throw std::runtime_error("OSSL_STORE_load failed: " + err);
    }

    if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_PKEY) {
      pkey = OSSL_STORE_INFO_get1_PKEY(info);  // increments refcount
      OSSL_STORE_INFO_free(info);
      break;
    }

    OSSL_STORE_INFO_free(info);
  }

  OSSL_STORE_close(store);

  if (!pkey) {
    throw std::runtime_error("No key found in PKCS#11 URI: " + uri);
  }
  return pkey;
}

}  // namespace

P11ContextWrapper::P11ContextWrapper(const boost::filesystem::path& module) {
  if (module.empty()) {
    ctx = nullptr;
    return;
  }
  // never returns NULL
  ctx = PKCS11_CTX_new();
  if (PKCS11_CTX_load(ctx, module.c_str()) != 0) {
    PKCS11_CTX_free(ctx);
    LOG_ERROR << "Couldn't load PKCS11 module " << module.string() << ": "
              << ERR_error_string(ERR_get_error(), nullptr);
    throw std::runtime_error("PKCS11 error");
  }
}

P11ContextWrapper::~P11ContextWrapper() {
  if (ctx != nullptr) {
    PKCS11_CTX_unload(ctx);
    PKCS11_CTX_free(ctx);
  }
}

P11SlotsWrapper::P11SlotsWrapper(PKCS11_ctx_st* ctx_in) {
  ctx = ctx_in;
  if (ctx == nullptr) {
    slots_ = nullptr;
    nslots = 0;
    return;
  }
  if (PKCS11_enumerate_slots(ctx, &slots_, &nslots) != 0) {
    LOG_ERROR << "Couldn't enumerate slots"
              << ": " << ERR_error_string(ERR_get_error(), nullptr);
    throw std::runtime_error("PKCS11 error");
  }
}

P11SlotsWrapper::~P11SlotsWrapper() {
  if ((slots_ != nullptr) && (nslots != 0U)) {
    PKCS11_release_all_slots(ctx, slots_, nslots);
  }
}

P11Engine::P11Engine(boost::filesystem::path module_path, std::string pass, std::string label)
    : module_path_(std::move(module_path)),
      pass_{std::move(pass)},
      label_{std::move(label)},
      ctx_(module_path_),
      wslots_(ctx_.get()) {
  if (module_path_.empty()) {
    return;
  }

  PKCS11_SLOT* slot = findTokenSlot();
  if ((slot == nullptr) || (slot->token == nullptr)) {
    throw std::runtime_error("Couldn't find pkcs11 token");
  }

  LOG_DEBUG << "Slot manufacturer......: " << slot->manufacturer;
  LOG_DEBUG << "Slot description.......: " << slot->description;
  LOG_DEBUG << "Slot token label.......: " << slot->token->label;
  LOG_DEBUG << "Slot token manufacturer: " << slot->token->manufacturer;
  LOG_DEBUG << "Slot token model.......: " << slot->token->model;
  LOG_DEBUG << "Slot token serialnr....: " << slot->token->serialnr;

  uri_prefix_ = std::string("pkcs11:serial=") + slot->token->serialnr + ";pin-value=" + pass_ + ";id=%";

  (void)providers();
}

// Hack for clang-tidy
#ifndef PKCS11_ENGINE_PATH
#define PKCS11_ENGINE_PATH "dummy"
#endif

boost::filesystem::path P11Engine::findPkcsLibrary() {
  static const boost::filesystem::path engine_path = PKCS11_ENGINE_PATH;
  if (!boost::filesystem::exists(engine_path)) {
    LOG_ERROR << "PKCS11 engine not available (" << engine_path << ")";
    return "";
  }
  return engine_path;
}

PKCS11_SLOT* P11Engine::findTokenSlot() const {
  PKCS11_SLOT* slot{nullptr};
  PKCS11_TOKEN* tok;
  const auto nslot{wslots_.get_nslots()};

  if (label_.empty()) {
    LOG_WARNING << "Token label missing. Using 1st initialized token.";
    slot = PKCS11_find_token(ctx_.get(), wslots_.get_slots(), wslots_.get_nslots());
  } else {
    auto iterslot{wslots_.get_slots()};
    for (unsigned int i = 0; i < nslot; i++, iterslot++) {
      if (iterslot != nullptr && (tok = iterslot->token) != nullptr) {
        if (label_ == tok->label) {
          slot = iterslot;
          break;
        }
      }
    }
  }
  if ((slot == nullptr) || (slot->token == nullptr)) {
    LOG_ERROR << "Couldn't find a token with label " << label_;
    return nullptr;
  }
  int rv;
  PKCS11_is_logged_in(slot, 1, &rv);
  if (rv == 0) {
    if (PKCS11_open_session(slot, 1) != 0) {
      LOG_ERROR << "Error creating rw session in to the slot: " << ERR_error_string(ERR_get_error(), nullptr);
    }

    if (PKCS11_login(slot, 0, pass_.c_str()) != 0) {
      LOG_ERROR << "Error logging in to the token: " << ERR_error_string(ERR_get_error(), nullptr);
      return nullptr;
    }
  }
  return slot;
}

bool P11Engine::readUptanePublicKey(const std::string& uptane_key_id, std::string* key_out) {
  if (module_path_.empty()) {
    return false;
  }
  if ((uptane_key_id.length() % 2) != 0U) {
    return false;
  }

  const std::string uri = uri_prefix_ + uptane_key_id;

  try {
    StructGuard<EVP_PKEY> evp_key(loadPublicKeyFromPkcs11Uri(uri), EVP_PKEY_free);

    StructGuard<BIO> mem(BIO_new(BIO_s_mem()), BIO_vfree);
    if (!PEM_write_bio_PUBKEY(mem.get(), evp_key.get())) {
      LOG_ERROR << "PEM_write_bio_PUBKEY failed: " << opensslErrOnce();
      return false;
    }

    char* pem_key = nullptr;
    // NOLINTNEXTLINE(google-runtime-int,cppcoreguidelines-pro-type-cstyle-cast)
    long length = BIO_get_mem_data(mem.get(), &pem_key);
    key_out->assign(pem_key, static_cast<size_t>(length));
    return true;

  } catch (const std::exception& e) {
    LOG_ERROR << "Failed reading PKCS#11 public key via provider: " << e.what();
    return false;
  }
}

bool P11Engine::generateUptaneKeyPair(const std::string& uptane_key_id) {
  PKCS11_SLOT* slot = findTokenSlot();
  if (slot == nullptr) {
    return false;
  }

  std::vector<unsigned char> id_hex;
  boost::algorithm::unhex(uptane_key_id, std::back_inserter(id_hex));

  StructGuard<EVP_PKEY> pkey = Crypto::generateRSAKeyPairEVP(KeyType::kRSA2048);
  if (pkey == nullptr) {
    LOG_ERROR << "Error generating keypair on the device:" << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }

  if (PKCS11_store_private_key(slot->token, pkey.get(), nullptr, id_hex.data(), id_hex.size()) != 0) {
    LOG_ERROR << "Could not store private key on the token";
    return false;
  }
  if (PKCS11_store_public_key(slot->token, pkey.get(), nullptr, id_hex.data(), id_hex.size()) != 0) {
    LOG_ERROR << "Could not store public key on the token";
    return false;
  }

  return true;
}

bool P11Engine::readTlsCert(const std::string& id, std::string* cert_out) const {
  if (module_path_.empty()) {
    return false;
  }
  if ((id.length() % 2) != 0U) {
    return false;  // id is a hex string
  }

  PKCS11_SLOT* slot = findTokenSlot();
  if (slot == nullptr) {
    return false;
  }

  PKCS11_CERT* certs;
  unsigned int ncerts;
  int rc = PKCS11_enumerate_certs(slot->token, &certs, &ncerts);
  if (rc < 0) {
    LOG_ERROR << "Error enumerating certificates in PKCS11 device: " << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }

  PKCS11_CERT* cert = nullptr;
  {
    std::vector<unsigned char> id_hex;
    boost::algorithm::unhex(id, std::back_inserter(id_hex));

    for (unsigned int i = 0; i < ncerts; i++) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      if ((certs[i].id_len == id.length() / 2) && (memcmp(certs[i].id, id_hex.data(), id.length() / 2) == 0)) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        cert = &certs[i];
        break;
      }
    }
  }
  if (cert == nullptr) {
    LOG_ERROR << "Requested certificate was not found";
    return false;
  }
  StructGuard<BIO> mem(BIO_new(BIO_s_mem()), BIO_vfree);
  PEM_write_bio_X509(mem.get(), cert->x509);

  char* pem_key = nullptr;
  // NOLINTNEXTLINE(google-runtime-int,cppcoreguidelines-pro-type-cstyle-cast)
  long length = BIO_get_mem_data(mem.get(), &pem_key);
  cert_out->assign(pem_key, static_cast<size_t>(length));

  return true;
}
