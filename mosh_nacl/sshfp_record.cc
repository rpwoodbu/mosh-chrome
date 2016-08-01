// sshfp_record.cc - DNS SSHFP record representation.

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

#include "mosh_nacl/sshfp_record.h"
#include "mosh_nacl/ssh.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using std::move;
using std::string;
using std::vector;

namespace {

SSHFPRecord::Fingerprint::Algorithm ConvertAlgorithm(
    const ssh::KeyType::KeyTypeEnum algorithm) {
  switch (algorithm) {
    case ssh::KeyType::RSA:
      return SSHFPRecord::Fingerprint::Algorithm::RSA;
    case ssh::KeyType::DSS:
      return SSHFPRecord::Fingerprint::Algorithm::DSA;
    case ssh::KeyType::ECDSA:
      return SSHFPRecord::Fingerprint::Algorithm::ECDSA;
    case ssh::KeyType::ED25519:
      return SSHFPRecord::Fingerprint::Algorithm::ED25519;
    default:
      return SSHFPRecord::Fingerprint::Algorithm::UNSET;
  }
}

string ParseHex(const string& hex) {
  string parsed;
  string hex_byte;
  for (const char hex_nibble : hex) {
    if (hex_nibble == ':' || hex_nibble == ' ') {
      // Skip over whitespace and delimiters.
      continue;
    }
    if (hex_byte.empty()) {
      hex_byte = hex_nibble;
      continue;
    }
    hex_byte += hex_nibble;
    parsed += strtol(hex_byte.c_str(), nullptr, 16);
    hex_byte.clear();
  }
  return parsed;
}

struct RdataParseResult {
  bool parsed_ok = false;
  int algorithm_num = -1;
  int type_num = -1;
  string fingerprint;
};

// Parse the "generic" RDATA form, which gives the wireform of the RDATA.
// This looks like:
//
//   \# ss xxxxxxxx...
//
// ... where "ss" is the size of the data in decimal, and "xx" is the data in
// hex.
RdataParseResult ParseGeneric(const string& rdata) {
  RdataParseResult result;

  if (rdata.find("\\# ") != 0) {
    // RDATA does not have "generic data" prefix.
    return result;
  }

  // Find whitespace after the prefix. We don't care about the size field; it
  // is implied by the amount of data in the data field.
  const size_t whitespace = rdata.find_first_of(" \t", 3);
  if (whitespace == string::npos) {
    return result;
  }
  const string data = ParseHex(rdata.substr(whitespace + 1));
  if (data.size() < 3) {
    // There must be at least 3 bytes: one for the algorithm, one for the
    // fingerprint type, and at least one for the fingerprint (although in
    // practice that's a ridiculously small fingerprint, but that's not for the
    // parser to determine).
    return result;
  }

  result.algorithm_num = data[0];
  result.type_num = data[1];
  result.fingerprint = data.substr(2);
  result.parsed_ok = true;
  return result;
}

// Parse the proper presentation form of the SSHFP RDATA. This looks like:
//
//   a b cccccccc...
//
// ... where "a" is the algorithm number, "b" is the fingerprint type, and "cc"
// is the fingerprint in hex.
RdataParseResult ParsePresentation(const string& rdata) {
  RdataParseResult result;

  const int scanned =
      sscanf(rdata.c_str(), "%d %d", &result.algorithm_num, &result.type_num);
  if (scanned != 2) {
    return result;
  }
  // Safely find the fingerprint part (using sscanf() for extracting into
  // strings is unsafe).
  const size_t first_whitespace = rdata.find_first_of(" \t");
  if (first_whitespace == string::npos) {
    return result;
  }
  const size_t second_whitespace =
      rdata.find_first_of(" \t", first_whitespace + 1);
  if (second_whitespace == string::npos) {
    return result;
  }
  result.fingerprint = ParseHex(rdata.substr(second_whitespace + 1));
  if (result.fingerprint.empty()) {
    return result;
  }

  result.parsed_ok = true;
  return result;
}

}  // anonymous namespace

bool SSHFPRecord::Parse(const vector<string>& rdata) {
  fingerprints_.clear();
  for (const auto& r : rdata) {
    Fingerprint fingerprint;
    if (!fingerprint.Parse(r)) {
      return false;
    }
    fingerprints_[fingerprint.algorithm()].push_back(move(fingerprint));
  }
  return true;
}

bool SSHFPRecord::IsValid(const ssh::Key& key) const {
  const auto key_algorithm = ConvertAlgorithm(key.GetKeyType().type());
  if (fingerprints_.count(key_algorithm) == 0) {
    // No SSHFP record for this key's algorithm.
    return false;
  }
  for (const auto& fingerprint : fingerprints_.at(key_algorithm)) {
    if (fingerprint.IsValid(key)) {
      return true;
    }
  }
  return false;
}

bool SSHFPRecord::Fingerprint::Parse(const string& rdata) {
  RdataParseResult parsed;

  parsed = ParseGeneric(rdata);
  if (!parsed.parsed_ok) {
    parsed = ParsePresentation(rdata);
  }
  if (!parsed.parsed_ok) {
    return false;
  }

  switch (parsed.algorithm_num) {
    case 0:
      algorithm_ = Algorithm::UNSET;
      break;
    case 1:
      algorithm_ = Algorithm::RSA;
      break;
    case 2:
      algorithm_ = Algorithm::DSA;
      break;
    case 3:
      algorithm_ = Algorithm::ECDSA;
      break;
    case 4:
      algorithm_ = Algorithm::ED25519;
      break;
    default:
      return false;
  }

  switch (parsed.type_num) {
    case 0:
      type_ = Type::RESERVED;
      break;
    case 1:
      type_ = Type::SHA1;
      break;
    case 2:
      type_ = Type::SHA256;
      break;
    default:
      return false;
  }

  fingerprint_ = move(parsed.fingerprint);

  return true;
}

bool SSHFPRecord::Fingerprint::IsMatchingAlgorithm(const ssh::Key& key) const {
  const auto key_algorithm = ConvertAlgorithm(key.GetKeyType().type());
  if (key_algorithm == Algorithm::UNSET) {
    return false;
  }
  return algorithm_ == key_algorithm;
}

bool SSHFPRecord::Fingerprint::IsValid(const ssh::Key& key) const {
  if (!IsMatchingAlgorithm(key)) {
    return false;
  }
  switch (type_) {
    case Type::SHA1:
      return ParseHex(key.SHA1()) == fingerprint_;
    // TODO: Support SHA256 (libssh doesn't have it).
    default:
      return false;
  }
}
