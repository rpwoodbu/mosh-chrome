// resolver.h - Interface for DNS lookups.

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

#ifndef MOSH_NACL_RESOLVER_H_
#define MOSH_NACL_RESOLVER_H_

#include <functional>
#include <string>
#include <vector>

class Resolver {
 public:
  Resolver() = default;
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;
  Resolver(Resolver&&) = default;
  Resolver& operator=(Resolver&&) = default;
  virtual ~Resolver() = default;

  // RRtype.
  enum class Type {
    A,
    AAAA,
    SSHFP,
  };

  // Error codes.
  enum class Error {
    OK,
    NOT_RESOLVED,
    TYPE_NOT_SUPPORTED,
    UNKNOWN,
  };

  // Whether the result is authentic.
  enum class Authenticity {
    // Verified authentic.
    AUTHENTIC,
    // Authenticity cannot be verified, because the zone is not secure (i.e.,
    // not DNSSEC). NB: A secure zone will never yield INSECURE; expect an
    // error instead.
    INSECURE,
  };

  // Type of the callback function.
  using Callback = std::function<void(Error error, Authenticity authenticity,
                                      std::vector<std::string> results)>;

  // Resolve |domain_name| to the given |type|. Returns immediately; calls
  // |callback| with result. If callback's |error| != Error::OK, |results| is
  // empty.
  //
  // |domain_name| is taken by value as a stable copy is needed. Callers can
  // std::move() it if desired for efficiency.
  virtual void Resolve(std::string domain_name, Type type,
                       Callback callback) = 0;

  // Whether this resolver validates responses (i.e. DNSSEC).
  virtual bool IsValidating() const = 0;

 protected:
  // Ensures the callback is always called. If class instance is deleted and
  // Call() hasn't been called, the callback will be called with a default
  // error code.
  class CallbackCaller {
   public:
    CallbackCaller() = default;
    explicit CallbackCaller(Callback callback) : callback_(callback) {}
    CallbackCaller(const CallbackCaller&) = delete;
    CallbackCaller& operator=(const CallbackCaller&) = delete;

    CallbackCaller(CallbackCaller&& orig) { callback_ = orig.Release(); }

    CallbackCaller& operator=(CallbackCaller&& orig) {
      Reset();
      callback_ = orig.Release();
      return *this;
    }

    ~CallbackCaller() { Reset(); }

    // Call the callback if there is one, and set this to nullptr.
    void Reset() {
      if (callback_ != nullptr) {
        callback_(Error::UNKNOWN, Authenticity::INSECURE, {});
      }
      callback_ = nullptr;
    }

    // Call the callback. Can only be called once. Afterward, this class will
    // not call the callback when deleted.
    void Call(Error error, Authenticity authenticity,
              std::vector<std::string> results) {
      Release()(error, authenticity, results);
    }

    // Release the callback; i.e., do not ever call the callback.
    Callback Release() {
      const auto callback = callback_;
      callback_ = nullptr;
      return callback;
    }

   private:
    Callback callback_;
  };
};

#endif  // MOSH_NACL_RESOLVER_H_
