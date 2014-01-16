// pepper_posix_native_udp.cc - Native Pepper UDP implementation.

// Copyright 2013 Richard Woodbury
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

#include "pepper_posix_native_udp.h"

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace PepperPOSIX {

NativeUDP::NativeUDP(
    const pp::InstanceHandle &instance_handle) :
    instance_handle_(instance_handle), factory_(this) {
  socket_ = new pp::UDPSocket(instance_handle);
  bound_ = false;
}

NativeUDP::~NativeUDP() {
  delete socket_;
}

int NativeUDP::Bind(const PP_NetAddress_IPv4 &address) {
  pp::NetAddress net_address(instance_handle_, address);
  pp::Var string_address = net_address.DescribeAsString(true);
  if (string_address.is_undefined()) {
    Log("NativeUDP::Bind() Address is bogus.");
    // TODO: Return something appropriate.
    return false;
  }

  int32_t result = socket_->Bind(net_address, pp::CompletionCallback());
  if (result == PP_OK) {
    bound_ = true;
    pp::Module::Get()->core()->CallOnMainThread(
        0, factory_.NewCallback(&NativeUDP::StartReceive));
  }
  // TODO: Flesh out error mapping.
  return result;
}

ssize_t NativeUDP::Send(
    const vector<char> &buf, int flags,
    const PP_NetAddress_IPv4 &address) {
  if (!bound_) {
    PP_NetAddress_IPv4 any_address;
    memset(&any_address, 0, sizeof(any_address));
    int result = Bind(any_address);
    if (result != 0) {
      Log("NativeUDP::Send(): Bind failed with %d", result);
      return 0;
    }
  }

  pp::NetAddress net_address(instance_handle_, address);
  return socket_->SendTo(
      buf.data(), buf.size(), net_address, pp::CompletionCallback());
}

// StartReceive prepares to receive another packet, and returns without
// blocking.
void NativeUDP::StartReceive(int32_t unused) {
  int32_t result = socket_->RecvFrom(
      receive_buffer_, sizeof(receive_buffer_),
      factory_.NewCallbackWithOutput(&NativeUDP::Received));
  if (result != PP_OK_COMPLETIONPENDING) {
    Log("NativeUDP::StartReceive(): RecvFrom returned %d", result);
    // TODO: Perhaps crash here?
  }
}

// Received is the callback result of StartReceive().
void NativeUDP::Received(int32_t result, const pp::NetAddress &address) {
  if (result < 0) {
    Log("NativeUDP::Received(%d, ...): Negative result; bailing.", result);
    return;
  }

  PP_NetAddress_IPv4 ipv4_addr;
  if (!address.DescribeAsIPv4Address(&ipv4_addr)) {
    // TODO: Implement IPv6 support, once mosh itself supports it.
    Log("NativeUDP::Received(): Failed to convert address.");
    return;
  }

  struct msghdr *message = (struct msghdr *)malloc(sizeof(struct msghdr));
  memset(message, 0, sizeof(*message));

  struct sockaddr_in *addr =
      (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
  addr->sin_family = AF_INET;
  addr->sin_port = ipv4_addr.port;
  uint32_t a = 0;
  for (int i = 0; i < 4; ++i) {
    a |= ipv4_addr.addr[i] << (8*i);
  }
  addr->sin_addr.s_addr = a;
  message->msg_name = addr;
  message->msg_namelen = sizeof(*addr);

  message->msg_iov = (struct iovec *)malloc(sizeof(struct iovec));
  message->msg_iovlen = 1;
  message->msg_iov->iov_base = malloc(result);
  message->msg_iov->iov_len = result;
  memcpy(
      message->msg_iov->iov_base, receive_buffer_, message->msg_iov->iov_len);

  AddPacket(message); // Takes ownership.

  // Await another packet.
  StartReceive(0);
}

// Close the socket.
int NativeUDP::Close() {
  // Destroying socket_ is the same as closing it.
  delete socket_;
  socket_ = NULL;
  return 0;
}

} // namespace PepperPOSIX
