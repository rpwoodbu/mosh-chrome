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

#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

using std::function;
using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

static const string gpdns_url = "https://dns.google.com/resolve";

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

} // anonymous namespace

void GPDNSResolver::Resolve(
    string domain_name,
    Type type,
    function<void(Error error, vector<string> results)> callback) {
  CallbackCaller caller(callback);

  if (IsNetworkAddress(domain_name)) {
    caller.Call(Error::OK, {domain_name});
    return;
  }

  string rr_type;
  switch (type) {
    case Type::A:
      rr_type = "A";
      break;
    case Type::AAAA:
      rr_type = "AAAA";
      break;
    case Type::SSHFP:
      rr_type = "SSHFP";
      break;
    // No default case; all enums accounted for.
  }

  const string url = gpdns_url + "?name=" + domain_name + "&type=" + rr_type;

  pp::URLRequestInfo request(instance_handle_);
  request.SetURL(url);
  request.SetMethod("GET");

  // Query is self-deleting.
  auto* query = new Query(instance_handle_);
  query->Run(move(request), move(caller));
}

void GPDNSResolver::Query::Run(
    pp::URLRequestInfo request, CallbackCaller caller) {
  unique_ptr<Query> deleter(this);
  caller_ = move(caller);
  deleter.release();
  loader_.Open(request, cc_factory_.NewCallback(&Query::OpenCallback));
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
  // TODO: Use a proper JSON parser.
  static const string answer_text = "\"Answer\":";
  const size_t answer_pos = response_.find(answer_text);
  if (answer_pos == string::npos) {
    // No answer. Does not exist.
    caller_.Call(Error::NOT_RESOLVED, {});
    return;
  }
  
  // Find matching brackets in the "Answer" section.
  const size_t answer_begin = response_.find('[', answer_pos);
  if (answer_begin == string::npos) {
    // Malformed response.
    return;
  }
  const size_t answer_end = response_.find(']', answer_pos);
  if (answer_end == string::npos) {
    // Malformed response.
    return;
  }

  ProcessAnswer(response_.substr(answer_begin + 1, answer_end - answer_begin));
  // This is the end. Let the deleter delete *this naturally.
}

void GPDNSResolver::Query::ProcessAnswer(const string& answer) {
  // Find all of the answers.
  vector<size_t> answer_positions;
  size_t cursor = 0;
  for (;;) {
    const size_t pos = answer.find('{', cursor);
    if (pos == string::npos) {
      break;
    }
    answer_positions.push_back(pos);
    cursor = pos + 1;
  }

  vector<string> results;

  for (const auto pos : answer_positions) {
    const size_t end_pos = answer.find('}', pos);
    if (end_pos == string::npos) {
      // Malformed response.
      return;
    }
    const string one_answer =
      ProcessOneAnswer(answer.substr(pos + 1, end_pos - pos));
    if (one_answer.size() == 0) {
      // Malformed response.
      return;
    }
    results.push_back(one_answer);
  }

  caller_.Call(Error::OK, move(results));
}

string GPDNSResolver::Query::ProcessOneAnswer(const string& answer) {
  const size_t data_pos = answer.find("\"data\"");
  if (data_pos == string::npos) {
    // Malformed response. Return the empty string.
    return "";
  }

  const size_t colon_pos = answer.find(':', data_pos);
  if (colon_pos == string::npos) {
    // Malformed response. Return the empty string.
    return "";
  }

  const size_t quote_pos = answer.find('"', colon_pos);
  if (quote_pos == string::npos) {
    // Malformed response. Return the empty string.
    return "";
  }

  const size_t close_quote_pos = answer.find('"', quote_pos + 1);
  if (close_quote_pos == string::npos) {
    // Malformed response. Return the empty string.
    return "";
  }

  return answer.substr(quote_pos + 1, close_quote_pos - quote_pos - 1);
}
