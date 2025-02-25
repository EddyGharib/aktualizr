#ifndef HTTPINTERFACE_H_
#define HTTPINTERFACE_H_

#include <future>
#include <string>
#include <utility>

#include <curl/curl.h>
#include "json/json.h"

#include "libaktualizr/types.h"
#include "libaktualizr/logging/logging.h"
#include "libaktualizr/utilities/utils.h"

using CurlHandler = std::shared_ptr<CURL>;

struct HttpResponse {
  HttpResponse() = default;
  HttpResponse(std::string body_in, const long http_status_code_in,  //  NOLINT(google-runtime-int)
               CURLcode curl_code_in, std::string error_message_in,
               std::unordered_map<std::string, std::string> &&headers_in = {})
      : body(std::move(body_in)),
        http_status_code(http_status_code_in),
        curl_code(curl_code_in),
        error_message(std::move(error_message_in)),
        headers{headers_in} {}
  std::string body;
  long http_status_code{0};  // NOLINT(google-runtime-int)
  CURLcode curl_code{CURLE_OK};
  std::string error_message;
  std::unordered_map<std::string, std::string> headers;
  bool isOk() const { return (curl_code == CURLE_OK && http_status_code >= 200 && http_status_code < 400); }
  bool wasInterrupted() const { return curl_code == CURLE_ABORTED_BY_CALLBACK; };
  std::string getStatusStr() const {
    return std::to_string(curl_code) + " " + error_message + " HTTP " + std::to_string(http_status_code);
  }

  Json::Value getJson() const { return Utils::parseJSON(body); }
};

class HttpInterface {
 public:
  HttpInterface() = default;
  virtual ~HttpInterface() = default;
  virtual HttpResponse get(const std::string &url, int64_t maxsize) = 0;
  virtual HttpResponse post(const std::string &url, const std::string &content_type, const std::string &data) = 0;
  virtual HttpResponse post(const std::string &url, const Json::Value &data) = 0;
  virtual HttpResponse put(const std::string &url, const std::string &content_type, const std::string &data) = 0;
  virtual HttpResponse put(const std::string &url, const Json::Value &data) = 0;

  virtual HttpResponse download(const std::string &url, curl_write_callback write_cb,
                                curl_xferinfo_callback progress_cb, void *userp, curl_off_t from) = 0;
  virtual std::future<HttpResponse> downloadAsync(const std::string &url, curl_write_callback write_cb,
                                                  curl_xferinfo_callback progress_cb, void *userp, curl_off_t from,
                                                  CurlHandler *easyp) = 0;
  virtual void setCerts(const std::string &ca, CryptoSource ca_source, const std::string &cert,
                        CryptoSource cert_source, const std::string &pkey, CryptoSource pkey_source) = 0;
  static constexpr int64_t kNoLimit = 0;  // no limit the size of downloaded data
  static constexpr int64_t kPostRespLimit = 64 * 1024;
  static constexpr int64_t kPutRespLimit = 64 * 1024;

 protected:
  HttpInterface(const HttpInterface &) = default;
  HttpInterface(HttpInterface &&) = default;
  HttpInterface &operator=(const HttpInterface &) = default;
  HttpInterface &operator=(HttpInterface &&) = default;
};

#endif  // HTTPINTERFACE_H_
