// gpdns_resolver.cc - The Google Public DNS-over-HTTPS resolver.

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

#include "gpdns_resolver.h"

#include <functional>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "json/reader.h"
#include "json/value.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

using std::function;
using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

static const string kGPDNSURL = "https://dns.google.com/resolve";

namespace {

bool IsNetworkAddress(const string& candidate) {
  {
    struct in_addr addr;
    if (inet_pton(AF_INET, candidate.c_str(), &addr) == 1) {
      return true;
    }
  }

  {
    struct in6_addr addr;
    if (inet_pton(AF_INET6, candidate.c_str(), &addr) == 1) {
      return true;
    }
  }

  return false;
}

int TypeToRRtype(Resolver::Type type) {
  switch (type) {
    case Resolver::Type::A:
      return 1;
    case Resolver::Type::AAAA:
      return 28;
    case Resolver::Type::SSHFP:
      return 44;
  }
}

string TypeToRRtypeStr(Resolver::Type type) {
  switch (type) {
    case Resolver::Type::A:
      return "A";
    case Resolver::Type::AAAA:
      return "AAAA";
    case Resolver::Type::SSHFP:
      return "SSHFP";
  }
}

} // anonymous namespace

void GPDNSResolver::Resolve(
    string domain_name,
    Type type,
    function<void(Error error, vector<string> results)> callback) {
  // Query is self-deleting.
  auto* query = new Query(
      instance_handle_, move(domain_name), type, CallbackCaller(callback));
  query->Run();
}

void GPDNSResolver::Query::Run() {
  unique_ptr<Query> deleter(this);

  if (IsNetworkAddress(domain_name_)) {
    caller_.Call(Error::OK, {domain_name_});
    return;
  }

  const string url =
      kGPDNSURL + "?name=" + domain_name_ + "&type=" + TypeToRRtypeStr(type_);

  request_.SetURL(url);
  request_.SetMethod("GET");

  deleter.release();
  loader_.Open(request_, cc_factory_.NewCallback(&Query::OpenCallback));
}

void GPDNSResolver::Query::OpenCallback(int32_t result) {
  unique_ptr<Query> deleter(this);

  if (result != PP_OK) {
    return;
  }

  pp::URLResponseInfo response = loader_.GetResponseInfo();
  if (response.GetStatusCode() != 200) {
    // TODO: Consider improving the error reporting.
    return;
  }

  ReadMore(move(deleter));
}

void GPDNSResolver::Query::ReadMore(unique_ptr<Query> deleter) {
  auto read_callback = cc_factory_.NewOptionalCallback(&Query::ReadCallback);

  int32_t read_result;
  do {
    read_result = loader_.ReadResponseBody(
        buffer_.data(), buffer_.size(), read_callback);
    if (read_result > 0) {
      // Things were fast, so we already have data.
      AppendDataBytes(read_result);
    }
  } while (read_result > 0);

  if (read_result != PP_OK_COMPLETIONPENDING) {
    deleter.release();
    read_callback.Run(read_result);
    return;
  }
  // Things were slow, so expect an async callback.
  deleter.release();
}

void GPDNSResolver::Query::AppendDataBytes(int32_t num_bytes) {
  response_.append(buffer_.data(), num_bytes);
}

void GPDNSResolver::Query::ReadCallback(int32_t result) {
  unique_ptr<Query> deleter(this);
  if (result == PP_OK) {
    ProcessResponse(move(deleter));
    return;
  }
  if (result > 0) {
    AppendDataBytes(result);
    ReadMore(move(deleter));
    return;
  }
  // An error occurred. Allow the deleter to delete *this, which will call the
  // callback with an error code.
}

void GPDNSResolver::Query::ProcessResponse(
    __attribute__((unused)) unique_ptr<Query> deleter) {
  Json::Reader reader;
  Json::Value parsed_json;
  if (!reader.parse(response_, parsed_json)) {
    // Malformed response.
    return;
  }

  const Json::Value answers = parsed_json["Answer"];
  if (answers.isNull()) {
    // No answer. Does not exist.
    caller_.Call(Error::NOT_RESOLVED, {});
    return;
  }

  vector<string> results;
  for (const Json::Value answer : answers) {
    if (!answer.isMember("type")) {
      // Malformed response.
      return;
    }
    if (answer.get("type", -1).asInt() != TypeToRRtype(type_)) {
      // Not the type we're looking for (e.g., CNAME).
      continue;
    }
    if (!answer.isMember("data")) {
      // Malformed response.
      return;
    }
    results.push_back(answer.get("data", "").asString());
  }

  if (results.size() == 0) {
    // NODATA response. Normally there's just an empty "Answer" section, but in
    // some cases (e.g., CNAME), there may be answers, just for different
    // RRtypes.
    caller_.Call(Error::NOT_RESOLVED, {});
    return;
  }

  caller_.Call(Error::OK, move(results));
}
