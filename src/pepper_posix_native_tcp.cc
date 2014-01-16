// pepper_posix_native_tcp.cc - Native Pepper TCP implementation.

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

#include "pepper_posix_native_tcp.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace PepperPOSIX {

NativeTCP::NativeTCP(
    const pp::InstanceHandle &instance_handle) :
    instance_handle_(instance_handle), factory_(this) {
  socket_ = new pp::TCPSocket(instance_handle);
}

NativeTCP::~NativeTCP() {
  delete socket_;
}

int NativeTCP::Bind(const PP_NetAddress_IPv4 &address) {
  pp::NetAddress net_address(instance_handle_, address);
  pp::Var string_address = net_address.DescribeAsString(true);
  if (string_address.is_undefined()) {
    Log("NativeTCP::Bind() Address is bogus.");
    // TODO: Return something appropriate.
    return false;
  }

  return socket_->Bind(net_address, pp::CompletionCallback());
}

int NativeTCP::Connect(const PP_NetAddress_IPv4 &address) {
  address_ = pp::NetAddress(instance_handle_, address);
  pp::Var string_address = address_.DescribeAsString(true);
  if (string_address.is_undefined()) {
    Log("NativeTCP::Connect() Address is bogus.");
    // TODO: Return something appropriate.
    return false;
  }
  // API calls need to be done on the main thread.
  pp::Module::Get()->core()->CallOnMainThread(
      0, factory_.NewCallback(&NativeTCP::ConnectOnMainThread));
  return EINPROGRESS;
}

// This callback should only be called on the main thread.
void NativeTCP::ConnectOnMainThread(int32_t unusued) {
  int32_t result = socket_->Connect(
      address_, factory_.NewCallback(&NativeTCP::Connected));
  if (result != PP_OK_COMPLETIONPENDING) {
    Log("NativeTCP::ConnectOnMainThread(): "
        "socket_->Connect() returned %d", result);
    // TODO: Perhaps crash here?
  }
  // TODO: Flesh out error mapping.
}

void NativeTCP::Connected(int32_t result) {
  if (result == PP_OK) {
    target_->UpdateWrite(true);
    StartReceive();
    return;
  }
  // TODO: Handle connection failures more appropraitely.
  Log("NativeTCP::Connected(): Connection failed; result: %d", result);
}

ssize_t NativeTCP::Send(const void *buf, size_t count, int flags) {
  if (flags != 0) {
    Log("NativeTCP::Send(): Unsupported flag: 0x%x", flags);
  }
  int32_t result = socket_->Write(
      (const char *)buf, count, pp::CompletionCallback());
  if (result < 0) {
    Log("NativeTCP::Send(): Got negative result: %d", result);
  }
  return result;
}

// StartReceive prepares to receive more data, and returns without blocking.
void NativeTCP::StartReceive() {
  int32_t result = socket_->Read(
      receive_buffer_, sizeof(receive_buffer_),
      factory_.NewCallback(&NativeTCP::Received));
  if (result != PP_OK_COMPLETIONPENDING) {
    Log("NativeTCP::StartReceive(): RecvFrom unexpectedly returned %d", result);
    // TODO: Perhaps crash here?
  }
}

// Received is the callback result of StartReceive().
void NativeTCP::Received(int32_t result) {
  if (result < 0) {
    Log("NativeTCP::Received(%d, ...): Negative result; bailing.", result);
    return;
  }
  AddData(receive_buffer_, result);
  // Await another packet.
  StartReceive();
}

// Close the socket.
int NativeTCP::Close() {
  // Destroying socket_ is the same as closing it.
  delete socket_;
  socket_ = NULL;
  return 0;
}

} // namespace PepperPOSIX
