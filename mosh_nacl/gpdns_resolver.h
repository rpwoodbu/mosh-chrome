// gpdns_resolver.h - The Google Public DNS-over-HTTPS resolver.

// Copyright 2016 Richard Woodbury
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef MOSH_NACL_GPDNS_RESOLVER_H_
#define MOSH_NACL_GPDNS_RESOLVER_H_

#include <memory>
#include <string>
#include <vector>

#include "mosh_nacl/resolver.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"

#include "ppapi/utility/completion_callback_factory.h"

class GPDNSResolver : public Resolver {
 public:
  GPDNSResolver() = delete;
  GPDNSResolver(pp::InstanceHandle handle) : instance_handle_(handle) {}
  GPDNSResolver(const GPDNSResolver&) = delete;
  GPDNSResolver& operator=(const GPDNSResolver&) = delete;
  GPDNSResolver(GPDNSResolver&&) = default;
  GPDNSResolver& operator=(GPDNSResolver&&) = default;
  virtual ~GPDNSResolver() = default;

  void Resolve(std::string domain_name, Type type, Callback callback) override;
  bool IsValidating() const override { return true; }

 private:
  // Encapsulates data and processing for a single query. Class is
  // self-deleting.
  class Query {
   public:
    Query(pp::InstanceHandle handle, std::string domain_name, Type type,
          CallbackCaller caller)
        : caller_(std::move(caller)),
          request_(handle),
          loader_(handle),
          buffer_(kBufferSize),
          domain_name_(std::move(domain_name)),
          type_(type),
          cc_factory_(this) {}

    // Do the query.
    void Run();

   private:
    // All private methods are run on the main thread.

    // Callback to switch execution to main thread (necessary to call Pepper
    // APIs).
    void RunOnMainThread(uint32_t unused);

    // Method that will be called when the URL is opened.
    void OpenCallback(int32_t result);

    // Read some data.
    void ReadMore(std::unique_ptr<Query> deleter);

    // Append |num_bytes| of data from |buffer_| into |respose_|.
    void AppendDataBytes(int32_t num_bytes);

    // Method that may be called when the URL is read.
    void ReadCallback(int32_t result);

    // Process the response.
    void ProcessResponse(std::unique_ptr<Query> deleter);

    // Buffer size for reading data from GPDNS.
    static const size_t kBufferSize = 16 * 1024;  // 16 kB

    CallbackCaller caller_;
    pp::URLRequestInfo request_;
    pp::URLLoader loader_;
    std::vector<char> buffer_;
    const std::string domain_name_;
    const Type type_;
    std::string response_;
    pp::CompletionCallbackFactory<Query> cc_factory_;
  };

  const pp::InstanceHandle instance_handle_;
};

#endif  // MOSH_NACL_GPDNS_RESOLVER_H_
