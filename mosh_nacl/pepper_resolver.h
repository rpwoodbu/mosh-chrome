// pepper_resolver.h - DNS resolver from the Pepper API.

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

#ifndef MOSH_NACL_PEPPER_RESOLVER_H_
#define MOSH_NACL_PEPPER_RESOLVER_H_

#include "mosh_nacl/resolver.h"

#include <functional>
#include <string>
#include <vector>

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/utility/completion_callback_factory.h"

class PepperResolver : public Resolver {
 public:
  PepperResolver() = delete;
  PepperResolver(pp::InstanceHandle handle)
      : resolver_(handle), cc_factory_(this) {}
  PepperResolver(const PepperResolver&) = delete;
  PepperResolver& operator=(const PepperResolver&) = delete;
  PepperResolver(PepperResolver&&) = default;
  PepperResolver& operator=(PepperResolver&&) = default;
  virtual ~PepperResolver() = default;

  void Resolve(std::string domain_name, Type type, Callback callback) override;
  bool IsValidating() const override { return false; }

 private:
  // Way to call |resolver_| from main thread.
  void ResolveOnMainThread(uint32_t unused, std::string domain_name,
                           PP_HostResolver_Hint hint, Callback callback);

  // Method that |resolver_| will callback.
  void ResolverCallback(int32_t result, Callback callback);

  pp::HostResolver resolver_;
  pp::CompletionCallbackFactory<PepperResolver> cc_factory_;
};

#endif  // MOSH_NACL_PEPPER_RESOLVER_H_
