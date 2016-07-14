// sshfp_record.h - DNS SSHFP record representation.

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

#ifndef SSHFP_RECORD_H
#define SSHFP_RECORD_H

#include <map>
#include <string>
#include <vector>

#include "ssh.h"

// Represents the SSHFP record from DNS in a way that is useful, e.g., for use
// by SSH login code.
class SSHFPRecord {
 public:
  SSHFPRecord() = default;
  SSHFPRecord(const SSHFPRecord&) = default;
  SSHFPRecord& operator=(const SSHFPRecord&) = default;
  SSHFPRecord(SSHFPRecord&&) = default;
  SSHFPRecord& operator=(SSHFPRecord&&) = default;
  ~SSHFPRecord() = default;

  // Parses the various SSHFP RDATA in either presentation format or "\#"
  // generic format. Returns false on parse error. Erases any previously parsed
  // data.
  bool Parse(const std::vector<std::string>& rdata);

  // Checks to see if |key| can be validated with any of the SSHFP RDATA
  // passed to Parse().
  bool IsValid(const ssh::Key& key) const;

  // Value type that represents one fingerprint from the SSHFP RRset.
  class Fingerprint {
   public:
    Fingerprint() = default;
    Fingerprint(const Fingerprint&) = default;
    Fingerprint& operator=(const Fingerprint&) = default;
    Fingerprint(Fingerprint&&) = default;
    Fingerprint& operator=(Fingerprint&&) = default;
    ~Fingerprint() = default;

    // Algorithm represented by the fingerprint.
    enum class Algorithm {
      UNSET,
      RESERVED,
      RSA,
      DSA,
      ECDSA,
      ED25519,
    };

    // Type of fingerprint.
    enum class Type {
      UNSET,
      RESERVED,
      SHA1,
      SHA256,
    };

    Algorithm algorithm() const { return algorithm_; }
    Type type() const { return type_; }
    std::string fingerprint() const { return fingerprint_; }

    // Parses one SSHFP RDATA in either presentation format or "\#" generic
    // format. Returns false on parse error. Erases any previously parsed data.
    bool Parse(const std::string& rdata);

    // Checks to see if the algorithm of |key| matches the algorithm of the
    // fingerprint.
    bool IsMatchingAlgorithm(const ssh::Key& key) const;

    // Checks to see if |key| can be validated with this fingerprint. Returns
    // false if IsMatchingAlgorithm() would return false.
    bool IsValid(const ssh::Key& key) const;

   private:
    Algorithm algorithm_ = Algorithm::UNSET;
    Type type_ = Type::UNSET;
    std::string fingerprint_;
  };

 private:
  std::map<Fingerprint::Algorithm, std::vector<Fingerprint>> fingerprints_;
};

#endif // SSHFP_RECORD_H
