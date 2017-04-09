// sshfp_record.h - DNS SSHFP record set representation.

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

#ifndef MOSH_NACL_SSHFP_RECORD_H_
#define MOSH_NACL_SSHFP_RECORD_H_

#include <map>
#include <string>
#include <vector>

#include "mosh_nacl/ssh.h"

// Represents the SSHFP record from DNS in a way that is useful, e.g., for use
// by SSH login code.
class SSHFPRecordSet {
 public:
  SSHFPRecordSet() = default;
  SSHFPRecordSet(const SSHFPRecordSet&) = default;
  SSHFPRecordSet& operator=(const SSHFPRecordSet&) = default;
  SSHFPRecordSet(SSHFPRecordSet&&) = default;
  SSHFPRecordSet& operator=(SSHFPRecordSet&&) = default;
  ~SSHFPRecordSet() = default;

  // Parses the various SSHFP RDATA in either presentation format or "\#"
  // generic format. Returns false on parse error. Erases any previously parsed
  // data.
  bool Parse(const std::vector<std::string>& rdata);

  enum class Validity {
    VALID,         // Found an SSHFP record that validates the key.
    INVALID,       // At least one SSHFP record does not validate the key.
    INSUFFICIENT,  // None of the SSHFP records could be used (e.g.,
                   // unsupported hash). Typically the client would continue as
                   // if no SSHFP record were published.
  };
  // Checks to see if |key| can be validated with any of the SSHFP RDATA
  // passed to Parse().
  Validity IsValid(const ssh::Key& key) const;

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
    // INSUFFICIENT if IsMatchingAlgorithm() would return false.
    Validity IsValid(const ssh::Key& key) const;

   private:
    Algorithm algorithm_ = Algorithm::UNSET;
    Type type_ = Type::UNSET;
    std::string fingerprint_;
  };

 private:
  std::map<Fingerprint::Algorithm, std::map<Fingerprint::Type, Fingerprint>>
      fingerprints_;
};

#endif  // MOSH_NACL_SSHFP_RECORD_H_
