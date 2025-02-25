#include "request_pool.h"

#include <algorithm>  // min
#include <chrono>
#include <exception>
#include <thread>

#include "libaktualizr/logging/logging.h"

RequestPool::RequestPool(TreehubServer& server, const int max_curl_requests, const RunMode mode, bool fsck_on_upload)
    : rate_controller_(max_curl_requests),
      running_requests_(0),
      server_(server),
      mode_(mode),
      fsck_on_upload_(fsck_on_upload),
      stopped_(false) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  multi_ = curl_multi_init();
  curl_multi_setopt(multi_, CURLMOPT_PIPELINING, CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX);
}

RequestPool::~RequestPool() {
  try {
    Abort();

    LOG_INFO << "Shutting down RequestPool...";
    while (!is_idle()) {
      LoopListen();
    }
    LOG_INFO << "...done";

    curl_multi_cleanup(multi_);
    curl_global_cleanup();
  } catch (std::exception& ex) {
    LOG_ERROR << "Exception in RequestPool dtor: " << ex.what();
  } catch (...) {
    LOG_ERROR << "Unknown exception in RequestPool dtor";
  }
}

void RequestPool::AddQuery(const OSTreeObject::ptr& request) {
  request->LaunchNotify();
  if (!stopped_) {
    query_queue_.push_back(request);
  }
}

void RequestPool::AddUpload(const OSTreeObject::ptr& request) {
  request->LaunchNotify();
  if (!stopped_) {
    upload_queue_.push_back(request);
  }
}

void RequestPool::LoopLaunch() {
  while (running_requests_ < rate_controller_.MaxConcurrency() && (!query_queue_.empty() || !upload_queue_.empty())) {
    OSTreeObject::ptr cur;

    // Queries first, uploads second
    if (query_queue_.empty()) {
      // Uploads
      cur = upload_queue_.front();
      upload_queue_.pop_front();
      // Check object's integrity before uploading them, but after we know they
      // are not present on the server
      if (fsck_on_upload_) {
        if (!cur->Fsck()) {
          LOG_ERROR << "Local object " << cur << " is corrupt. Aborting upload.";
          Abort();
          continue;
        }
      }
      cur->Upload(server_, multi_, mode_);
      put_requests_made_++;
      total_object_size_ += cur->GetSize();
      if (mode_ == RunMode::kDryRun || mode_ == RunMode::kWalkTree) {
        // Don't send an actual upload message, just skip to the part where we
        // acknowledge that the object has been uploaded.
        cur->NotifyParents(*this);
      }
    } else {
      // Queries
      cur = query_queue_.front();
      query_queue_.pop_front();
      cur->MakeTestRequest(server_, multi_);
      head_requests_made_++;
    }

    running_requests_++;
  }
}

void RequestPool::LoopListen() {
  // For more information about the timeout logic, read these:
  // https://curl.haxx.se/libcurl/c/curl_multi_timeout.html
  // https://curl.haxx.se/libcurl/c/curl_multi_fdset.html
  CURLMcode mc;
  // Poll for IO
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcept;
  int maxfd = 0;
  FD_ZERO(&fdread);    // NOLINT(readability-isolate-declaration,cppcoreguidelines-pro-bounds-constant-array-index)
  FD_ZERO(&fdwrite);   // NOLINT(readability-isolate-declaration,cppcoreguidelines-pro-bounds-constant-array-index)
  FD_ZERO(&fdexcept);  // NOLINT(readability-isolate-declaration,cppcoreguidelines-pro-bounds-constant-array-index)
  long timeoutms = 0;  // NOLINT(google-runtime-int)
  mc = curl_multi_timeout(multi_, &timeoutms);
  if (mc != CURLM_OK) {
    throw std::runtime_error("curl_multi_timeout failed with error");
  }
  // If timeoutms is 0, "it means you should proceed immediately without waiting
  // for anything".
  if (timeoutms != 0) {
    mc = curl_multi_fdset(multi_, &fdread, &fdwrite, &fdexcept, &maxfd);
    if (mc != CURLM_OK) {
      throw std::runtime_error("curl_multi_fdset failed with error");
    }

    struct timeval timeout {};
    if (maxfd != -1) {
      // "Wait for activities no longer than the set timeout."
      if (timeoutms == -1) {
        // "You must not wait too long (more than a few seconds perhaps)".
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
      } else {
        timeout.tv_sec = timeoutms / 1000;
        timeout.tv_usec = 1000 * (timeoutms % 1000);
      }
      if (select(maxfd + 1, &fdread, &fdwrite, &fdexcept, &timeout) < 0) {
        throw std::runtime_error(std::string("select failed with error: ") + std::strerror(errno));
      }
    } else if (timeoutms > 0) {
      // If maxfd == -1, then wait the lesser of timeoutms and 100 ms.
      long nofd_timeoutms = std::min(timeoutms, static_cast<long>(100));  // NOLINT(google-runtime-int)
      LOG_DEBUG << "Waiting " << nofd_timeoutms << " ms for curl";
      timeout.tv_sec = 0;
      timeout.tv_usec = 1000 * (nofd_timeoutms % 1000);
      if (select(0, nullptr, nullptr, nullptr, &timeout) < 0) {
        throw std::runtime_error(std::string("select failed with error: ") + std::strerror(errno));
      }
    }
  }

  // Ask curl to handle IO
  mc = curl_multi_perform(multi_, &running_requests_);
  if (mc != CURLM_OK) {
    throw std::runtime_error("curl_multi failed with error");
  }
  assert(running_requests_ >= 0);

  // Deal with any completed requests
  int msgs_in_queue;
  do {
    CURLMsg* msg = curl_multi_info_read(multi_, &msgs_in_queue);
    if ((msg != nullptr) && msg->msg == CURLMSG_DONE) {
      OSTreeObject::ptr completed_object = ostree_object_from_curl(msg->easy_handle);
      completed_object->CurlDone(multi_, *this);
      auto start_time = completed_object->RequestStartTime();
      auto end_time = RateController::clock::now();
      bool server_responded_ok = completed_object->LastOperationResult() == ServerResponse::kOk;
      rate_controller_.RequestCompleted(start_time, end_time, server_responded_ok);

      if (rate_controller_.ServerHasFailed()) {
        Abort();
      } else {
        auto duration = rate_controller_.GetSleepTime();
        if (duration > RateController::clock::duration(0)) {
          LOG_DEBUG << "Sleeping for " << std::chrono::duration_cast<std::chrono::seconds>(duration).count()
                    << " seconds due to server congestion.";
          std::this_thread::sleep_for(duration);
        }
      }
    }
  } while (msgs_in_queue > 0);
}

void RequestPool::Loop() {
  LoopLaunch();
  LoopListen();
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
