#include "libaktualizr/http/httpclient.h"

#include <cassert>
#include <sstream>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "libaktualizr/utilities/utils.h"

struct WriteStringArg {
  std::string out;
  int64_t limit{0};
};

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. It handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeString(void* contents, size_t size, size_t nmemb, void* userp) {
  assert(contents);
  assert(userp);
  // append the writeback data to the provided string
  auto* arg = static_cast<WriteStringArg*>(userp);
  if (arg->limit > 0) {
    if (arg->out.length() + size * nmemb > static_cast<uint64_t>(arg->limit)) {
      return 0;
    }
  }
  (static_cast<WriteStringArg*>(userp))->out.append(static_cast<char*>(contents), size * nmemb);

  // return size of written data
  return size * nmemb;
}

struct ResponseHeaders {
  explicit ResponseHeaders(const std::set<std::string>& header_names_in) : header_names{header_names_in} {}
  const std::set<std::string>& header_names;
  std::unordered_map<std::string, std::string> headers;
};

static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* const resp_headers{reinterpret_cast<ResponseHeaders*>(userdata)};
  std::string header{buffer};

  if (resp_headers == nullptr) {
    LOG_WARNING << "Failed to cast the header callback context to `ResponseHeaders*`";
    return nitems * size;
  }

  const auto split_pos{header.find(':')};
  if (std::string::npos != split_pos) {
    std::string header_name{header.substr(0, split_pos)};
    std::string header_value{header.substr(split_pos + 1)};
    boost::trim_if(header_name, boost::is_any_of(" \t\r\n"));
    boost::trim_if(header_value, boost::is_any_of(" \t\r\n"));

    boost::algorithm::to_lower(header_name);
    boost::algorithm::to_lower(header_value);

    if (resp_headers->header_names.end() != resp_headers->header_names.find(header_name)) {
      resp_headers->headers[header_name] = header_value;
    }
  }
  return nitems * size;
}

HttpClient::HttpClient(const std::vector<std::string>* extra_headers,
                       const std::set<std::string>* response_header_names) {
  if (response_header_names != nullptr) {
    for (const auto& name : *response_header_names) {
      response_header_names_.emplace(boost::to_lower_copy(name));
    }
  }

  curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error("Could not initialize curl");
  }
  headers = nullptr;

  curlEasySetoptWrapper(curl, CURLOPT_NOSIGNAL, 1L);
  curlEasySetoptWrapper(curl, CURLOPT_TIMEOUT, 60L);
  curlEasySetoptWrapper(curl, CURLOPT_CONNECTTIMEOUT, 60L);
  curlEasySetoptWrapper(curl, CURLOPT_CAPATH, Utils::getCaPath());

  curlEasySetoptWrapper(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curlEasySetoptWrapper(curl, CURLOPT_MAXREDIRS, 10L);
  curlEasySetoptWrapper(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_301);

  // let curl use our write function
  curlEasySetoptWrapper(curl, CURLOPT_WRITEFUNCTION, writeString);
  curlEasySetoptWrapper(curl, CURLOPT_WRITEDATA, NULL);

  curlEasySetoptWrapper(curl, CURLOPT_VERBOSE, get_curlopt_verbose());

  headers = curl_slist_append(headers, "Accept: */*");

  if (extra_headers != nullptr) {
    for (const auto& header : *extra_headers) {
      headers = curl_slist_append(headers, header.c_str());
    }
  }
  curlEasySetoptWrapper(curl, CURLOPT_USERAGENT, Utils::getUserAgent());
}

HttpClient::HttpClient(const std::string& socket) : HttpClient() {
  curlEasySetoptWrapper(curl, CURLOPT_UNIX_SOCKET_PATH, socket.c_str());
}

HttpClient::HttpClient(const HttpClient& curl_in)
    : HttpInterface(curl_in), pkcs11_key(curl_in.pkcs11_key), pkcs11_cert(curl_in.pkcs11_key) {
  curl = curl_easy_duphandle(curl_in.curl);
  headers = curl_slist_dup(curl_in.headers);
}

const CurlGlobalInitWrapper HttpClient::manageCurlGlobalInit_{};

HttpClient::~HttpClient() {
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
}

void HttpClient::setCerts(const std::string& ca, CryptoSource ca_source, const std::string& cert,
                          CryptoSource cert_source, const std::string& pkey, CryptoSource pkey_source) {
  curlEasySetoptWrapper(curl, CURLOPT_SSL_VERIFYPEER, 1);
  curlEasySetoptWrapper(curl, CURLOPT_SSL_VERIFYHOST, 2);
  curlEasySetoptWrapper(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

  if (ca_source == CryptoSource::kPkcs11) {
    throw std::runtime_error("Accessing CA certificate on PKCS11 devices isn't currently supported");
  }
  std::unique_ptr<TemporaryFile> tmp_ca_file = std_::make_unique<TemporaryFile>("tls-ca");
  tmp_ca_file->PutContents(ca);
  curlEasySetoptWrapper(curl, CURLOPT_CAINFO, tmp_ca_file->Path().c_str());
  tls_ca_file = std::move_if_noexcept(tmp_ca_file);

  if (cert_source == CryptoSource::kPkcs11) {
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERT, cert.c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERTTYPE, "ENG");
  } else {  // cert_source == CryptoSource::kFile
    std::unique_ptr<TemporaryFile> tmp_cert_file = std_::make_unique<TemporaryFile>("tls-cert");
    tmp_cert_file->PutContents(cert);
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERT, tmp_cert_file->Path().c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERTTYPE, "PEM");
    tls_cert_file = std::move_if_noexcept(tmp_cert_file);
  }
  pkcs11_cert = (cert_source == CryptoSource::kPkcs11);

  if (pkey_source == CryptoSource::kPkcs11) {
    curlEasySetoptWrapper(curl, CURLOPT_SSLENGINE, "pkcs11");
    curlEasySetoptWrapper(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEY, pkey.c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEYTYPE, "ENG");
  } else {  // pkey_source == CryptoSource::kFile
    std::unique_ptr<TemporaryFile> tmp_pkey_file = std_::make_unique<TemporaryFile>("tls-pkey");
    tmp_pkey_file->PutContents(pkey);
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEY, tmp_pkey_file->Path().c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEYTYPE, "PEM");
    tls_pkey_file = std::move_if_noexcept(tmp_pkey_file);
  }
  pkcs11_key = (pkey_source == CryptoSource::kPkcs11);
}

HttpResponse HttpClient::get(const std::string& url, int64_t maxsize) {
  CURL* curl_get = dupHandle(curl, pkcs11_key);

  curlEasySetoptWrapper(curl_get, CURLOPT_HTTPHEADER, headers);

  if (pkcs11_cert) {
    curlEasySetoptWrapper(curl_get, CURLOPT_SSLCERTTYPE, "ENG");
  }

  // Clear POSTFIELDS to remove any lingering references to strings that have
  // probably since been deallocated.
  curlEasySetoptWrapper(curl_get, CURLOPT_POSTFIELDS, "");
  curlEasySetoptWrapper(curl_get, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_get, CURLOPT_HTTPGET, 1L);
  LOG_DEBUG << "GET " << url;
  HttpResponse response = perform(curl_get, RETRY_TIMES, maxsize);
  curl_easy_cleanup(curl_get);
  return response;
}

HttpResponse HttpClient::post(const std::string& url, const std::string& content_type, const std::string& data) {
  CURL* curl_post = dupHandle(curl, pkcs11_key);
  curl_slist* req_headers = curl_slist_dup(headers);
  req_headers = curl_slist_append(req_headers, (std::string("Content-Type: ") + content_type).c_str());
  curlEasySetoptWrapper(curl_post, CURLOPT_HTTPHEADER, req_headers);
  curlEasySetoptWrapper(curl_post, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_post, CURLOPT_POST, 1);
  curlEasySetoptWrapper(curl_post, CURLOPT_POSTFIELDS, data.c_str());
  curlEasySetoptWrapper(curl_post, CURLOPT_POSTFIELDSIZE, data.size());
  auto result = perform(curl_post, RETRY_TIMES, HttpInterface::kPostRespLimit);
  curl_easy_cleanup(curl_post);
  curl_slist_free_all(req_headers);
  return result;
}

HttpResponse HttpClient::post(const std::string& url, const Json::Value& data) {
  std::string data_str = Utils::jsonToCanonicalStr(data);
  LOG_TRACE << "post request body:" << data;
  return post(url, "application/json", data_str);
}

HttpResponse HttpClient::put(const std::string& url, const std::string& content_type, const std::string& data) {
  CURL* curl_put = dupHandle(curl, pkcs11_key);
  curl_slist* req_headers = curl_slist_dup(headers);
  req_headers = curl_slist_append(req_headers, (std::string("Content-Type: ") + content_type).c_str());
  curlEasySetoptWrapper(curl_put, CURLOPT_HTTPHEADER, req_headers);
  curlEasySetoptWrapper(curl_put, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_put, CURLOPT_POSTFIELDS, data.c_str());
  curlEasySetoptWrapper(curl_put, CURLOPT_CUSTOMREQUEST, "PUT");
  HttpResponse result = perform(curl_put, RETRY_TIMES, HttpInterface::kPutRespLimit);
  curl_easy_cleanup(curl_put);
  curl_slist_free_all(req_headers);
  return result;
}

HttpResponse HttpClient::put(const std::string& url, const Json::Value& data) {
  std::string data_str = Utils::jsonToCanonicalStr(data);
  LOG_TRACE << "put request body:" << data;
  return put(url, "application/json", data_str);
}

// NOLINTNEXTLINE(misc-no-recursion)
HttpResponse HttpClient::perform(CURL* curl_handler, int retry_times, int64_t size_limit) {
  if (size_limit >= 0) {
    // it will only take effect if the server declares the size in advance,
    //    writeString callback takes care of the other case
    curlEasySetoptWrapper(curl_handler, CURLOPT_MAXFILESIZE_LARGE, size_limit);
  }
  curlEasySetoptWrapper(curl_handler, CURLOPT_LOW_SPEED_TIME, speed_limit_time_interval_);
  curlEasySetoptWrapper(curl_handler, CURLOPT_LOW_SPEED_LIMIT, speed_limit_bytes_per_sec_);

  WriteStringArg response_arg;
  response_arg.limit = size_limit;
  curlEasySetoptWrapper(curl_handler, CURLOPT_WRITEDATA, static_cast<void*>(&response_arg));
  ResponseHeaders resp_headers(response_header_names_);
  if (!resp_headers.header_names.empty()) {
    curlEasySetoptWrapper(curl_handler, CURLOPT_HEADERDATA, &resp_headers);
    curlEasySetoptWrapper(curl_handler, CURLOPT_HEADERFUNCTION, header_callback);
  }
  CURLcode result = curl_easy_perform(curl_handler);
  long http_code;  // NOLINT(google-runtime-int)
  curl_easy_getinfo(curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
  HttpResponse response(response_arg.out, http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "",
                        std::move(resp_headers.headers));
  if (response.curl_code != CURLE_OK || response.http_status_code >= 500) {
    std::ostringstream error_message;
    error_message << "curl error " << response.curl_code << " (http code " << response.http_status_code
                  << "): " << response.error_message;
    LOG_ERROR << error_message.str();
    if (retry_times != 0) {
      sleep(1);
      // NOLINTNEXTLINE(misc-no-recursion)
      response = perform(curl_handler, --retry_times, size_limit);
    }
  }
  LOG_TRACE << "response http code: " << response.http_status_code;
  LOG_TRACE << "response: " << response.body;
  return response;
}

HttpResponse HttpClient::download(const std::string& url, curl_write_callback write_cb,
                                  curl_xferinfo_callback progress_cb, void* userp, curl_off_t from) {
  return downloadAsync(url, write_cb, progress_cb, userp, from, nullptr).get();
}

std::future<HttpResponse> HttpClient::downloadAsync(const std::string& url, curl_write_callback write_cb,
                                                    curl_xferinfo_callback progress_cb, void* userp, curl_off_t from,
                                                    CurlHandler* easyp) {
  CURL* curl_download = dupHandle(curl, pkcs11_key);

  CurlHandler curlp = CurlHandler(curl_download, curl_easy_cleanup);

  if (easyp != nullptr) {
    *easyp = curlp;
  }

  curlEasySetoptWrapper(curl_download, CURLOPT_HTTPHEADER, headers);
  curlEasySetoptWrapper(curl_download, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_download, CURLOPT_HTTPGET, 1L);
  curlEasySetoptWrapper(curl_download, CURLOPT_WRITEFUNCTION, write_cb);
  curlEasySetoptWrapper(curl_download, CURLOPT_WRITEDATA, userp);
  if (progress_cb != nullptr) {
    curlEasySetoptWrapper(curl_download, CURLOPT_NOPROGRESS, 0);
    curlEasySetoptWrapper(curl_download, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curlEasySetoptWrapper(curl_download, CURLOPT_XFERINFODATA, userp);
  }
  curlEasySetoptWrapper(curl_download, CURLOPT_TIMEOUT, 0);
  curlEasySetoptWrapper(curl_download, CURLOPT_LOW_SPEED_TIME, speed_limit_time_interval_);
  curlEasySetoptWrapper(curl_download, CURLOPT_LOW_SPEED_LIMIT, speed_limit_bytes_per_sec_);
  curlEasySetoptWrapper(curl_download, CURLOPT_RESUME_FROM_LARGE, from);

  std::promise<HttpResponse> resp_promise;
  auto resp_future = resp_promise.get_future();
  std::thread(
      [curlp, this](std::promise<HttpResponse> promise) {
        ResponseHeaders resp_headers(response_header_names_);
        if (!resp_headers.header_names.empty()) {
          curlEasySetoptWrapper(curlp.get(), CURLOPT_HEADERDATA, &resp_headers);
          curlEasySetoptWrapper(curlp.get(), CURLOPT_HEADERFUNCTION, header_callback);
        }
        CURLcode result = curl_easy_perform(curlp.get());
        long http_code;  // NOLINT(google-runtime-int)
        curl_easy_getinfo(curlp.get(), CURLINFO_RESPONSE_CODE, &http_code);
        HttpResponse response("", http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "",
                              std::move(resp_headers.headers));
        promise.set_value(response);
      },
      std::move(resp_promise))
      .detach();
  return resp_future;
}

bool HttpClient::updateHeader(const std::string& name, const std::string& value) {
  curl_slist* item = headers;
  std::string lookfor(name + ":");
  std::string lookfor_empty(name + ";");

  while (item != nullptr) {
    if (strncmp(lookfor.c_str(), item->data, lookfor.length()) == 0 ||
        strncmp(lookfor_empty.c_str(), item->data, lookfor_empty.length()) == 0) {
      free(item->data);  // NOLINT(cppcoreguidelines-no-malloc, hicpp-no-malloc)
      std::string new_value{name};
      if (!value.empty()) {
        new_value += ": " + value;
      } else {
        // this is the way to make libcurl send a request header with an empty value in the `<header>:` format.
        new_value += ";";
      }
      item->data = strdup(new_value.c_str());
      return true;
    }
    item = item->next;
  }
  return false;
}

void HttpClient::timeout(int64_t ms) {
  // curl_easy_setopt() takes a 'long' be very sure that we are passing
  // whatever the platform ABI thinks is a long, while keeping the external
  // interface a clang-tidy preferred int64
  auto ms_long = static_cast<long>(ms);  // NOLINT(google-runtime-int)
  curlEasySetoptWrapper(curl, CURLOPT_TIMEOUT_MS, ms_long);
  curlEasySetoptWrapper(curl, CURLOPT_CONNECTTIMEOUT_MS, ms_long);
}

curl_slist* HttpClient::curl_slist_dup(curl_slist* sl) {
  curl_slist* new_list = nullptr;

  for (curl_slist* item = sl; item != nullptr; item = item->next) {
    new_list = curl_slist_append(new_list, item->data);
  }

  return new_list;
}

/* Locking for curl share instance */
static void curl_share_lock_cb(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr) {
  (void)handle;
  (void)access;
  auto* mutexes = static_cast<std::array<std::mutex, CURL_LOCK_DATA_LAST>*>(userptr);
  mutexes->at(data).lock();
}

static void curl_share_unlock_cb(CURL* handle, curl_lock_data data, void* userptr) {
  (void)handle;
  auto* mutexes = static_cast<std::array<std::mutex, CURL_LOCK_DATA_LAST>*>(userptr);
  mutexes->at(data).unlock();
}

void HttpClientWithShare::initCurlShare() {
  share_ = curl_share_init();
  if (share_ == nullptr) {
    throw std::runtime_error("Could not initialize share");
  }

  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share_, CURLSHOPT_LOCKFUNC, curl_share_lock_cb);
  curl_share_setopt(share_, CURLSHOPT_UNLOCKFUNC, curl_share_unlock_cb);
  curl_share_setopt(share_, CURLSHOPT_USERDATA, &curl_share_mutexes);
}

HttpClientWithShare::HttpClientWithShare(const std::vector<std::string>* extra_headers,
                                         const std::set<std::string>* response_header_names)
    : HttpClient(extra_headers, response_header_names) {
  initCurlShare();
}
HttpClientWithShare::HttpClientWithShare(const std::string& socket) : HttpClient(socket) { initCurlShare(); }

HttpClientWithShare::HttpClientWithShare(const HttpClientWithShare& curl_in) : HttpClient(curl_in) { initCurlShare(); }

HttpClientWithShare::~HttpClientWithShare() { curl_share_cleanup(share_); }
// vim: set tabstop=2 shiftwidth=2 expandtab:
