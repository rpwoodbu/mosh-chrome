// pepper_posix_native_udp.h - Native Pepper UDP implementation.

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

#ifndef PEPPER_POSIX_NATIVE_TCP_HPP
#define PEPPER_POSIX_NATIVE_TCP_HPP

#include "pepper_posix_tcp.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/utility/completion_callback_factory.h"

const int TCP_RECEIVE_BUFFER_SIZE = 64*1024; // 64 kB, a decent window size.

namespace PepperPOSIX {

// NativeTCP implements TCP using the native Pepper TCPSockets API.
class NativeTCP : public TCP {
 public:
  NativeTCP(const pp::InstanceHandle &instance_handle);
  virtual ~NativeTCP();

  // Bind replaces bind().
  virtual int Bind(const PP_NetAddress_IPv4 &address);

  // Connect replaces connect().
  virtual int Connect(const PP_NetAddress_IPv4 &address);

  // Send replaces send().
  virtual ssize_t Send(const void *buf, size_t count, int flags);

  // Close replaces close().
  virtual int Close();

 private:
  void ConnectOnMainThread(int32_t unused);
  void Connected(int32_t result);
  void StartReceive();
  void Received(int32_t result);

  pp::TCPSocket *socket_;
  const pp::InstanceHandle &instance_handle_;
  char receive_buffer_[TCP_RECEIVE_BUFFER_SIZE];
  pp::CompletionCallbackFactory<NativeTCP> factory_;
  pp::NetAddress address_;

  // Disable copy and assignment.
  NativeTCP(const NativeTCP&);
  NativeTCP &operator=(const NativeTCP&);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX_NATIVE_TCP_HPP
