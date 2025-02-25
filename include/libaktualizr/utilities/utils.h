#ifndef UTILS_H_
#define UTILS_H_

#include <boost/filesystem/path.hpp>
#include <memory>
#include <string>

#include <curl/curl.h>
#include <netinet/in.h>

#include "json/json.h"

struct Utils {
  static std::string fromBase64(std::string base64_string);
  static std::string toBase64(const std::string &tob64);
  static std::string stripQuotes(const std::string &value);
  static std::string addQuotes(const std::string &value);
  static std::string extractField(const std::string &in, unsigned int field_id);
  static Json::Value parseJSON(const std::string &json_str);
  static Json::Value parseJSONFile(const boost::filesystem::path &filename);
  static std::string jsonToStr(const Json::Value &json);
  static std::string jsonToCanonicalStr(const Json::Value &json);
  static std::string genPrettyName();
  static std::string readFile(const boost::filesystem::path &filename, bool trim = false);

  static void writeFile(const boost::filesystem::path &filename, const char *content, size_t size);
  static void writeFile(const boost::filesystem::path &filename, const std::string &content,
                        bool create_directories = true);
  static void writeFile(const boost::filesystem::path &filename, const Json::Value &content,
                        bool create_directories = true);
  static void copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to);
  static std::string readFileFromArchive(std::istream &as, const std::string &filename, bool trim = false);
  static void writeArchive(const std::map<std::string, std::string> &entries, std::ostream &as);
  static void removeFileFromArchive(const boost::filesystem::path &archive_path, const std::string &filename);
  static Json::Value getHardwareInfo();
  static Json::Value getNetworkInfo();
  static std::string getHostname();
  static std::string randomUuid();
  static sockaddr_storage ipGetSockaddr(int fd);
  static std::string ipDisplayName(const sockaddr_storage &saddr);
  static int ipPort(const sockaddr_storage &saddr);
  static int shell(const std::string &command, std::string *output, bool include_stderr = false);
  static boost::filesystem::path absolutePath(const boost::filesystem::path &root, const boost::filesystem::path &file);
  static void createDirectories(const boost::filesystem::path &path, mode_t mode);
  static bool createSecureDirectory(const boost::filesystem::path &path);
  static std::string urlEncode(const std::string &input);
  static CURL *curlDupHandleWrapper(CURL *curl_in, bool using_pkcs11, CURLSH *share);
  static std::vector<boost::filesystem::path> getDirEntriesByExt(const boost::filesystem::path &dir_path,
                                                                 const std::string &ext);
  static void setStorageRootPath(const std::string &storage_root_path);
  static boost::filesystem::path getStorageRootPath();

  static void setUserAgent(std::string user_agent);
  static const char *getUserAgent();

  static void setCaPath(boost::filesystem::path path);
  static const char *getCaPath();

 private:
  static std::string storage_root_path_;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static std::string user_agent_;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static boost::filesystem::path ca_path_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

/**
 * RAII Temporary file creation
 */
class TemporaryFile {
 public:
  explicit TemporaryFile(const std::string &hint = "file");
  ~TemporaryFile();
  TemporaryFile(const TemporaryFile &guard) = delete;
  TemporaryFile(TemporaryFile &&) = delete;
  TemporaryFile &operator=(const TemporaryFile &guard) = delete;
  TemporaryFile &operator=(TemporaryFile &&) = delete;
  void PutContents(const std::string &contents) const;
  boost::filesystem::path Path() const;
  std::string PathString() const;

 private:
  boost::filesystem::path tmp_name_;
};

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(const std::string &hint = "dir");
  ~TemporaryDirectory();
  TemporaryDirectory(const TemporaryDirectory &guard) = delete;
  TemporaryDirectory(TemporaryDirectory &&) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &guard) = delete;
  TemporaryDirectory &operator=(TemporaryDirectory &&) = delete;
  boost::filesystem::path Path() const;
  std::string PathString() const;
  boost::filesystem::path operator/(const boost::filesystem::path &subdir) const;

 private:
  boost::filesystem::path tmp_name_;
};

// helper template for C (mostly openssl) data structured
//   user should still take care about the order of destruction
//   by instantiating StructGuard<> in a right order.
//   BTW local variables are destructed in reverse order of instantiation
template <typename T>
using StructGuard = std::unique_ptr<T, void (*)(T *)>;
template <typename T>
using StructGuardInt = std::unique_ptr<T, int (*)(T *)>;

class Socket {
 public:
  Socket();
  explicit Socket(int fd) : socket_fd_(fd) {}
  virtual ~Socket();
  Socket(const Socket &guard) = delete;
  Socket(Socket &&) = delete;
  Socket &operator=(const Socket &guard) = delete;
  Socket &operator=(Socket &&) = delete;

  int &operator*() { return socket_fd_; }
  std::string ToString() const;

 protected:
  void bind(in_port_t port, bool reuse = true) const;

  int socket_fd_;
};

class ConnectionSocket : public Socket {
 public:
  ConnectionSocket(const std::string &ip, in_port_t port, in_port_t bind_port = 0);
  ~ConnectionSocket() override;
  ConnectionSocket(const ConnectionSocket &guard) = delete;
  ConnectionSocket(ConnectionSocket &&) = delete;
  ConnectionSocket &operator=(const ConnectionSocket &guard) = delete;
  ConnectionSocket &operator=(ConnectionSocket &&) = delete;

  int connect();

 private:
  struct sockaddr_in remote_sock_address_;
};

class ListenSocket : public Socket {
 public:
  explicit ListenSocket(in_port_t port);
  in_port_t port() const { return _port; }

 private:
  in_port_t _port;
};

// wrapper for curl handles
class CurlEasyWrapper {
 public:
  CurlEasyWrapper();
  ~CurlEasyWrapper();
  CurlEasyWrapper(const CurlEasyWrapper &guard) = delete;
  CurlEasyWrapper(CurlEasyWrapper &&) = delete;
  CurlEasyWrapper &operator=(const CurlEasyWrapper &guard) = delete;
  CurlEasyWrapper &operator=(CurlEasyWrapper &&) = delete;
  CURL *get() { return handle; }

 private:
  CURL *handle;
};

template <typename... T>
static void curlEasySetoptWrapper(CURL *curl_handle, CURLoption option, T &&...args) {
  const CURLcode retval = curl_easy_setopt(curl_handle, option, std::forward<T>(args)...);
  if (retval != 0U) {
    throw std::runtime_error(std::string("curl_easy_setopt error: ") + curl_easy_strerror(retval));
  }
}

// this is reference implementation of make_unique which is not yet included to C++11
namespace std_ {
template <class T>
struct _Unique_if {                           // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
  using _Single_object = std::unique_ptr<T>;  // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
};

template <class T>
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
struct _Unique_if<T[]> {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
  using _Unknown_bound = std::unique_ptr<T[]>;
};

template <class T, size_t N>
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
struct _Unique_if<T[N]> {
  using _Known_bound = void;  // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args &&...args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
  using U = typename std::remove_extent<T>::type;
  return std::unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args &&...) = delete;
}  // namespace std_

#endif  // UTILS_H_
