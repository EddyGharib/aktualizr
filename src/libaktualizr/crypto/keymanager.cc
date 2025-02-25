#include "libaktualizr/crypto/keymanager.h"

#include <stdexcept>
#include <utility>

#include <boost/filesystem.hpp>
#include <boost/scoped_array.hpp>

#include "crypto/openssl_compat.h"
#include "libaktualizr/crypto/crypto.h"
#include "libaktualizr/crypto/p11engine.h"
#include "libaktualizr/http/httpinterface.h"
#include "libaktualizr/storage/invstorage.h"
#include "libaktualizr/types.h"

// by using constexpr the compiler can optimize out method calls when the
// feature is disabled. We won't then need to link with the actual p11 engine
// implementation
#ifdef BUILD_P11
static constexpr bool built_with_p11 = true;
#else
static constexpr bool built_with_p11 = false;
#endif

KeyManager::KeyManager(std::shared_ptr<INvStorage> backend, KeyManagerConfig config,
                       const std::shared_ptr<P11EngineGuard> &p11)
    : backend_(std::move(backend)), config_(std::move(config)) {
  if (built_with_p11) {
    if (!p11) {
      p11_ = std::make_shared<P11EngineGuard>(config_.p11.module, config_.p11.pass, config_.p11.label);
    } else {
      p11_ = p11;
    }
  }
}

void KeyManager::loadKeys(const std::string *pkey_content, const std::string *cert_content,
                          const std::string *ca_content) {
  if (config_.tls_pkey_source == CryptoSource::kFile) {
    std::string pkey;
    if (pkey_content != nullptr) {
      pkey = *pkey_content;
    } else {
      backend_->loadTlsPkey(&pkey);
    }
    if (!pkey.empty()) {
      if (tmp_pkey_file == nullptr) {
        tmp_pkey_file = std_::make_unique<TemporaryFile>("tls-pkey");
      }
      tmp_pkey_file->PutContents(pkey);
    }
  }
  if (config_.tls_cert_source == CryptoSource::kFile) {
    std::string cert;
    if (cert_content != nullptr) {
      cert = *cert_content;
    } else {
      backend_->loadTlsCert(&cert);
    }
    if (!cert.empty()) {
      if (tmp_cert_file == nullptr) {
        tmp_cert_file = std_::make_unique<TemporaryFile>("tls-cert");
      }
      tmp_cert_file->PutContents(cert);
    }
  }
  if (config_.tls_ca_source == CryptoSource::kFile) {
    std::string ca;
    if (ca_content != nullptr) {
      ca = *ca_content;
    } else {
      backend_->loadTlsCa(&ca);
    }
    if (!ca.empty()) {
      if (tmp_ca_file == nullptr) {
        tmp_ca_file = std_::make_unique<TemporaryFile>("tls-ca");
      }
      tmp_ca_file->PutContents(ca);
    }
  }
}

std::string KeyManager::getPkeyFile() const {
  std::string pkey_file;
  if (config_.tls_pkey_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    pkey_file = (*p11_)->getItemFullId(config_.p11.tls_pkey_id);
  }
  if (config_.tls_pkey_source == CryptoSource::kFile) {
    if (tmp_pkey_file && !boost::filesystem::is_empty(tmp_pkey_file->PathString())) {
      pkey_file = tmp_pkey_file->PathString();
    }
  }
  return pkey_file;
}

std::string KeyManager::getCertFile() const {
  std::string cert_file;
  if (config_.tls_cert_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    cert_file = (*p11_)->getItemFullId(config_.p11.tls_clientcert_id);
  }
  if (config_.tls_cert_source == CryptoSource::kFile) {
    if (tmp_cert_file && !boost::filesystem::is_empty(tmp_cert_file->PathString())) {
      cert_file = tmp_cert_file->PathString();
    }
  }
  return cert_file;
}

std::string KeyManager::getCaFile() const {
  std::string ca_file;
  if (config_.tls_ca_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    ca_file = (*p11_)->getItemFullId(config_.p11.tls_cacert_id);
  }
  if (config_.tls_ca_source == CryptoSource::kFile) {
    if (tmp_ca_file && !boost::filesystem::is_empty(tmp_ca_file->PathString())) {
      ca_file = tmp_ca_file->PathString();
    }
  }
  return ca_file;
}

std::string KeyManager::getPkey() const {
  std::string pkey;
  if (config_.tls_pkey_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    pkey = (*p11_)->getItemFullId(config_.p11.tls_pkey_id);
  }
  if (config_.tls_pkey_source == CryptoSource::kFile) {
    backend_->loadTlsPkey(&pkey);
  }
  return pkey;
}

std::string KeyManager::getCert() const {
  std::string cert;
  if (config_.tls_cert_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    cert = (*p11_)->getItemFullId(config_.p11.tls_clientcert_id);
  }
  if (config_.tls_cert_source == CryptoSource::kFile) {
    backend_->loadTlsCert(&cert);
  }
  return cert;
}

std::string KeyManager::getCa() const {
  std::string ca;
  if (config_.tls_ca_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    ca = (*p11_)->getItemFullId(config_.p11.tls_cacert_id);
  }
  if (config_.tls_ca_source == CryptoSource::kFile) {
    backend_->loadTlsCa(&ca);
  }
  return ca;
}

std::string KeyManager::getCN() const {
  const std::string not_found_cert_message = "Certificate is not found, can't extract device_id";
  std::string cert;
  if (config_.tls_cert_source == CryptoSource::kFile) {
    if (!backend_->loadTlsCert(&cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  } else {  // CryptoSource::kPkcs11
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11 support, can't extract device_id");
    }
    if (!(*p11_)->readTlsCert(config_.p11.tls_clientcert_id, &cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  }

  return Crypto::extractSubjectCN(cert);
}

std::string KeyManager::getBC() const {
  const std::string not_found_cert_message = "Certificate is not found, can't extract device_id";
  std::string cert;
  if (config_.tls_cert_source == CryptoSource::kFile) {
    if (!backend_->loadTlsCert(&cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  } else {  // CryptoSource::kPkcs11
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11 support, can't extract device_id");
    }
    if (!(*p11_)->readTlsCert(config_.p11.tls_clientcert_id, &cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  }
  return Crypto::extractSubjectBC(cert);
}

void KeyManager::getCertInfo(std::string *subject, std::string *issuer, std::string *not_before,
                             std::string *not_after) const {
  std::string not_found_cert_message = "Certificate is not found, can't extract device certificate";
  std::string cert;
  if (config_.tls_cert_source == CryptoSource::kFile) {
    if (!backend_->loadTlsCert(&cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  } else {  // CryptoSource::kPkcs11
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11 support, can't extract device certificate");
    }
    if (!(*p11_)->readTlsCert(config_.p11.tls_clientcert_id, &cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  }

  StructGuard<BIO> bio(BIO_new_mem_buf(const_cast<char *>(cert.c_str()), static_cast<int>(cert.size())), BIO_vfree);
  StructGuard<X509> x(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
  if (x == nullptr) {
    throw std::runtime_error("Could not parse certificate");
  }

  StructGuard<BIO> subj_bio(BIO_new(BIO_s_mem()), BIO_vfree);
  X509_NAME_print_ex(subj_bio.get(), X509_get_subject_name(x.get()), 1, 0);
  char *subj_buf = nullptr;
  auto subj_len = BIO_get_mem_data(subj_bio.get(), &subj_buf);  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
  if (subj_buf == nullptr) {
    throw std::runtime_error("Could not parse certificate subject");
  }
  *subject = std::string(subj_buf, static_cast<size_t>(subj_len));

  StructGuard<BIO> issuer_bio(BIO_new(BIO_s_mem()), BIO_vfree);
  X509_NAME_print_ex(issuer_bio.get(), X509_get_issuer_name(x.get()), 1, 0);
  char *issuer_buf = nullptr;
  auto issuer_len = BIO_get_mem_data(issuer_bio.get(), &issuer_buf);  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
  if (issuer_buf == nullptr) {
    throw std::runtime_error("Could not parse certificate issuer");
  }
  *issuer = std::string(issuer_buf, static_cast<size_t>(issuer_len));

#if AKTUALIZR_OPENSSL_PRE_11
  const ASN1_TIME *nb_asn1 = X509_get_notBefore(x.get());
#else
  const ASN1_TIME *nb_asn1 = X509_get0_notBefore(x.get());
#endif
  StructGuard<BIO> nb_bio(BIO_new(BIO_s_mem()), BIO_vfree);
  ASN1_TIME_print(nb_bio.get(), nb_asn1);
  char *nb_buf;
  auto nb_len = BIO_get_mem_data(nb_bio.get(), &nb_buf);  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
  *not_before = std::string(nb_buf, static_cast<size_t>(nb_len));

#if AKTUALIZR_OPENSSL_PRE_11
  const ASN1_TIME *na_asn1 = X509_get_notAfter(x.get());
#else
  const ASN1_TIME *na_asn1 = X509_get0_notAfter(x.get());
#endif
  StructGuard<BIO> na_bio(BIO_new(BIO_s_mem()), BIO_vfree);
  ASN1_TIME_print(na_bio.get(), na_asn1);
  char *na_buf;
  auto na_len = BIO_get_mem_data(na_bio.get(), &na_buf);  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
  *not_after = std::string(na_buf, static_cast<size_t>(na_len));
}

void KeyManager::copyCertsToCurl(HttpInterface &http) const {
  std::string pkey = getPkey();
  std::string cert = getCert();
  std::string ca = getCa();

  if (!pkey.empty() && !cert.empty() && !ca.empty()) {
    http.setCerts(ca, config_.tls_ca_source, cert, config_.tls_cert_source, pkey, config_.tls_pkey_source);
  }
}

Json::Value KeyManager::signTuf(const Json::Value &in_data) const {
  ENGINE *crypto_engine = nullptr;
  std::string private_key;
  if (config_.uptane_key_source == CryptoSource::kPkcs11) {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11");
    }
    crypto_engine = (*p11_)->getEngine();
    private_key = config_.p11.uptane_key_id;
  }

  std::string b64sig;
  if (config_.uptane_key_source == CryptoSource::kFile) {
    backend_->loadPrimaryPrivate(&private_key);
  }
  b64sig = Utils::toBase64(
      Crypto::Sign(config_.uptane_key_type, crypto_engine, private_key, Utils::jsonToCanonicalStr(in_data)));

  Json::Value signature;
  switch (config_.uptane_key_type) {
    case KeyType::kRSA2048:
    case KeyType::kRSA3072:
    case KeyType::kRSA4096:
      signature["method"] = "rsassa-pss";
      break;
    case KeyType::kED25519:
      signature["method"] = "ed25519";
      break;
    default:
      throw std::runtime_error("Unknown key type");
  }
  signature["sig"] = b64sig;

  Json::Value out_data;
  signature["keyid"] = UptanePublicKey().KeyId();
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return out_data;
}

std::string KeyManager::generateUptaneKeyPair() {
  std::string primary_public;

  if (config_.uptane_key_source == CryptoSource::kFile) {
    std::string primary_private;
    if (!backend_->loadPrimaryKeys(&primary_public, &primary_private)) {
      bool result_ = Crypto::generateKeyPair(config_.uptane_key_type, &primary_public, &primary_private);
      if (result_) {
        backend_->storePrimaryKeys(primary_public, primary_private);
      }
    }
    if (primary_public.empty() && primary_private.empty()) {
      throw std::runtime_error("Could not get Uptane keys");
    }
  } else {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11 support!");
    }
    // dummy read to check if the key is present
    if (!(*p11_)->readUptanePublicKey(config_.p11.uptane_key_id, &primary_public)) {
      (*p11_)->generateUptaneKeyPair(config_.p11.uptane_key_id);
    }
    // really read the key
    if (primary_public.empty() && !(*p11_)->readUptanePublicKey(config_.p11.uptane_key_id, &primary_public)) {
      throw std::runtime_error("Could not get Uptane keys");
    }
  }
  return primary_public;
}

PublicKey KeyManager::UptanePublicKey() const {
  std::string primary_public;
  if (config_.uptane_key_source == CryptoSource::kFile) {
    if (!backend_->loadPrimaryPublic(&primary_public)) {
      throw std::runtime_error("Could not get Uptane public key!");
    }
  } else {
    if (!built_with_p11) {
      throw std::runtime_error("Aktualizr was built without PKCS#11 support!");
    }
    // dummy read to check if the key is present
    if (!(*p11_)->readUptanePublicKey(config_.p11.uptane_key_id, &primary_public)) {
      throw std::runtime_error("Could not get Uptane public key!");
    }
  }
  return PublicKey(primary_public, config_.uptane_key_type);
}
