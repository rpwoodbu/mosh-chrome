// sshfp_record.cc - DNS SSHFP record set representation.

// Copyright 2016, 2017 Richard Woodbury
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

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "mosh_nacl/ssh.h"

using std::move;
using std::string;
using std::vector;

namespace {

// The list of all fingerprint types in decreasing order of priority.
// Fingerprints of higher priority are chosen if they exist.
// See: https://tools.ietf.org/html/rfc6594#section-4.1
static const auto kFingerprintPriority = {
    SSHFPRecordSet::Fingerprint::Type::SHA256,
    SSHFPRecordSet::Fingerprint::Type::SHA1,
};

SSHFPRecordSet::Fingerprint::Algorithm ConvertAlgorithm(
    const ssh::KeyType::KeyTypeEnum algorithm) {
  switch (algorithm) {
    case ssh::KeyType::RSA:
      return SSHFPRecordSet::Fingerprint::Algorithm::RSA;
    case ssh::KeyType::DSS:
      return SSHFPRecordSet::Fingerprint::Algorithm::DSA;
    case ssh::KeyType::ECDSA:
      return SSHFPRecordSet::Fingerprint::Algorithm::ECDSA;
    case ssh::KeyType::ED25519:
      return SSHFPRecordSet::Fingerprint::Algorithm::ED25519;
    default:
      return SSHFPRecordSet::Fingerprint::Algorithm::UNSET;
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

bool SSHFPRecordSet::Parse(const vector<string>& rdata) {
  fingerprints_.clear();
  for (const auto& r : rdata) {
    Fingerprint fingerprint;
    if (!fingerprint.Parse(r)) {
      return false;
    }
    fingerprints_[fingerprint.algorithm()].emplace(fingerprint.type(),
                                                   move(fingerprint));
  }
  return true;
}

SSHFPRecordSet::Validity SSHFPRecordSet::IsValid(const ssh::Key& key) const {
  const auto key_algorithm = ConvertAlgorithm(key.GetKeyType().type());
  if (fingerprints_.count(key_algorithm) == 0) {
    // No SSHFP record for this key's algorithm.
    return Validity::INSUFFICIENT;
  }
  const auto& fingerprints_by_type = fingerprints_.at(key_algorithm);
  for (const auto& type : kFingerprintPriority) {
    const auto& fingerprint_iter = fingerprints_by_type.find(type);
    if (fingerprint_iter == fingerprints_by_type.end()) {
      // That fingerprint type doesn't exist. Try the next one in the list.
      continue;
    }
    const auto& fingerprint = fingerprint_iter->second;
    switch (fingerprint.IsValid(key)) {
      case Validity::VALID:
        // Accept the first valid fingerprint.
        return Validity::VALID;
      case Validity::INVALID:
        // Any invalid fingerprint is grounds for concern.
        return Validity::INVALID;
      case Validity::INSUFFICIENT:
        // Do nothing. This fingerprint isn't usable (perhaps because the hash
        // isn't supported).
        break;
        // No default case; compiler will complain if some enum values are
        // missing.
    }
  }
  // We didn't actually invalidate anything, perhaps because none of the
  // advertised hashes were supported.
  return Validity::INSUFFICIENT;
}

bool SSHFPRecordSet::Fingerprint::Parse(const string& rdata) {
  RdataParseResult parsed;

  parsed = ParseGeneric(rdata);
  if (!parsed.parsed_ok) {
    parsed = ParsePresentation(rdata);
  }
  if (!parsed.parsed_ok) {
    return false;
  }

  switch (parsed.algorithm_num) {
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
      algorithm_ = Algorithm::UNSET;
      break;
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
      type_ = Type::UNSET;
      break;
  }

  fingerprint_ = move(parsed.fingerprint);

  return true;
}

bool SSHFPRecordSet::Fingerprint::IsMatchingAlgorithm(
    const ssh::Key& key) const {
  const auto key_algorithm = ConvertAlgorithm(key.GetKeyType().type());
  if (key_algorithm == Algorithm::UNSET) {
    return false;
  }
  return algorithm_ == key_algorithm;
}

SSHFPRecordSet::Validity SSHFPRecordSet::Fingerprint::IsValid(
    const ssh::Key& key) const {
  if (!IsMatchingAlgorithm(key)) {
    return Validity::INSUFFICIENT;
  }

  switch (type_) {
    case Type::SHA1:
      if (ParseHex(key.SHA1()) == fingerprint_) {
        return Validity::VALID;
      } else {
        return Validity::INVALID;
      }

    // TODO(rpwoodbu): Support SHA256 (libssh doesn't have it).

    default:
      return Validity::INSUFFICIENT;
  }
}
