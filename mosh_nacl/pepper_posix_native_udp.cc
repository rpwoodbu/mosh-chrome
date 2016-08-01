// pepper_posix_native_udp.cc - Native Pepper UDP implementation.

// Copyright 2013, 2014, 2015 Richard Woodbury
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

#include "mosh_nacl/pepper_posix_native_udp.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <memory>
#include <vector>

#include "mosh_nacl/make_unique.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace PepperPOSIX {

using std::vector;
using util::make_unique;

NativeUDP::NativeUDP(const pp::InstanceHandle instance_handle)
    : socket_(new pp::UDPSocket(instance_handle)),
      instance_handle_(instance_handle),
      factory_(this) {}

NativeUDP::~NativeUDP() {}

int NativeUDP::Bind(const pp::NetAddress& address) {
  pp::Var string_address = address.DescribeAsString(true);
  if (string_address.is_undefined()) {
    Log("NativeUDP::Bind() Address is bogus.");
    // TODO(rpwoodbu): Return something appropriate.
    return false;
  }

  int32_t result = socket_->Bind(address, pp::CompletionCallback());
  if (result == PP_OK) {
    bound_ = true;
    pp::Module::Get()->core()->CallOnMainThread(
        0, factory_.NewCallback(&NativeUDP::StartReceive));
  }
  // TODO(rpwoodbu): Flesh out error mapping.
  return result;
}

ssize_t NativeUDP::Send(const vector<char>& buf,
                        __attribute__((unused)) int flags,
                        const pp::NetAddress& address) {
  if (!bound_) {
    const auto family = address.GetFamily();
    int result;

    if (family == PP_NETADDRESS_FAMILY_IPV4) {
      PP_NetAddress_IPv4 any_v4_address;
      memset(&any_v4_address, 0, sizeof(any_v4_address));
      result = Bind(pp::NetAddress(instance_handle_, any_v4_address));
    } else if (family == PP_NETADDRESS_FAMILY_IPV6) {
      PP_NetAddress_IPv6 any_v6_address;
      memset(&any_v6_address, 0, sizeof(any_v6_address));
      result = Bind(pp::NetAddress(instance_handle_, any_v6_address));
    } else {
      Log("NativeUDP::Send(): Unknown address family: %d", family);
      return 0;
    }

    if (result != 0) {
      Log("NativeUDP::Send(): Bind failed with %d", result);
      return 0;
    }
  }

  int32_t result = socket_->SendTo(buf.data(), buf.size(), address,
                                   pp::CompletionCallback());
  if (result < 0) {
    switch (result) {
      case PP_ERROR_ADDRESS_UNREACHABLE:
        errno = EHOSTUNREACH;
        break;
      default:
        // Set errno to something, even if it isn't precise.
        Log("NativeUDP::Send(): socket_->SendTo() failed with %d", result);
        errno = EIO;
        break;
    }
  }
  return result;
}

// StartReceive prepares to receive another packet, and returns without
// blocking.
void NativeUDP::StartReceive(__attribute__((unused)) int32_t unused) {
  int32_t result =
      socket_->RecvFrom(receive_buffer_, sizeof(receive_buffer_),
                        factory_.NewCallbackWithOutput(&NativeUDP::Received));
  if (result != PP_OK_COMPLETIONPENDING) {
    Log("NativeUDP::StartReceive(): RecvFrom returned %d", result);
    // TODO(rpwoodbu): Perhaps crash here?
  }
}

// Received is the callback result of StartReceive().
void NativeUDP::Received(int32_t result, const pp::NetAddress& address) {
  if (result < 0) {
    Log("NativeUDP::Received(%d, ...): Negative result; bailing.", result);
    return;
  }
  AddPacket(make_unique<MsgHdr>(address, result, receive_buffer_));
  // Await another packet.
  StartReceive(0);
}

// Close the socket.
int NativeUDP::Close() {
  // Destroying socket_ is the same as closing it.
  socket_.reset();
  return 0;
}

}  // namespace PepperPOSIX
