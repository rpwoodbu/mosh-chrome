// pepper_posix_udp.cc - UDP Pepper POSIX adapters.
//
// Pepper POSIX is a set of adapters to enable POSIX-like APIs to work with the
// callback-based APIs of Pepper (and transitively, JavaScript).

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

#include "mosh_nacl/pepper_posix_udp.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

namespace PepperPOSIX {

using std::unique_ptr;
using std::vector;

MsgHdr::MsgHdr(const pp::NetAddress& addr, int32_t size,
               const char* const buf) {
  memset(this, 0, sizeof(*this));

  switch (addr.GetFamily()) {
    case PP_NETADDRESS_FAMILY_IPV4: {
      PP_NetAddress_IPv4 ipv4_addr;
      assert(addr.DescribeAsIPv4Address(&ipv4_addr));
      struct sockaddr_in* saddr =
          (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
      saddr->sin_family = AF_INET;
      saddr->sin_port = ipv4_addr.port;
      uint32_t a = 0;
      for (int i = 0; i < 4; ++i) {
        a |= ipv4_addr.addr[i] << (8 * i);
      }
      saddr->sin_addr.s_addr = a;
      msg_name = saddr;
      msg_namelen = sizeof(*saddr);
    } break;

    case PP_NETADDRESS_FAMILY_IPV6: {
      PP_NetAddress_IPv6 ipv6_addr;
      assert(addr.DescribeAsIPv6Address(&ipv6_addr));
      struct sockaddr_in6* saddr =
          (struct sockaddr_in6*)malloc(sizeof(struct sockaddr_in6));
      saddr->sin6_family = AF_INET6;
      saddr->sin6_port = ipv6_addr.port;
      memcpy(saddr->sin6_addr.s6_addr, ipv6_addr.addr,
             sizeof(saddr->sin6_addr.s6_addr));
      msg_name = saddr;
      msg_namelen = sizeof(*saddr);
    } break;

    default:
      // Unsupported address family.
      assert(false);
      break;
  }

  msg_iov = (struct iovec*)malloc(sizeof(struct iovec));
  msg_iovlen = 1;
  msg_iov->iov_base = malloc(size);
  msg_iov->iov_len = size;
  memcpy(msg_iov->iov_base, buf, msg_iov->iov_len);
}

MsgHdr::~MsgHdr() {
  free(msg_name);
  for (int i = 0; i < msg_iovlen; ++i) {
    free(msg_iov[i].iov_base);
    free(msg_iov + i);
  }
  assert(msg_control == nullptr);  // This isn't being used.
}

UDP::UDP() {}

UDP::~UDP() {
  // There really shouldn't be another thread actively involved at destruction
  // time, but getting the lock nonetheless.
  pthread::MutexLock m(packets_lock_);
  packets_.clear();
}

ssize_t UDP::Receive(struct ::msghdr* message,
                     __attribute((unused)) int flags) {
  unique_ptr<MsgHdr> latest;

  {
    pthread::MutexLock m(packets_lock_);
    if (packets_.size() == 0) {
      errno = EWOULDBLOCK;
      return -1;
    }
    latest = move(packets_.front());
    packets_.pop_front();
    target_->UpdateRead(packets_.size() > 0);
  }

  if (message->msg_namelen >= latest->msg_namelen) {
    memcpy(message->msg_name, latest->msg_name, latest->msg_namelen);
  } else {
    Log("UDP::Receive(): msg_namelen too short.");
  }

  assert(latest->msg_iovlen == 1);  // For simplicity, as this is internal.
  ssize_t size = 0;
  size_t input_len = latest->msg_iov->iov_len;
  for (int i = 0; i < message->msg_iovlen && size < input_len; ++i) {
    size_t output_len = message->msg_iov[i].iov_len;
    size_t to_copy = output_len <= input_len ? output_len : input_len;
    memcpy(message->msg_iov[i].iov_base, latest->msg_iov->iov_base, to_copy);
    size += to_copy;
  }
  assert(size ==
         input_len);  // TODO(rpwoodbu): Return a real error, or handle better.

  // TODO(rpwoodbu): Ignoring flags, msg_flags, and msg_control for now.

  return size;
}

void UDP::AddPacket(unique_ptr<MsgHdr> message) {
  {
    pthread::MutexLock m(packets_lock_);
    packets_.push_back(move(message));
  }
  target_->UpdateRead(true);
}

ssize_t StubUDP::Send(const vector<char>& buf,
                      __attribute__((unused)) int flags,
                      __attribute__((unused)) const pp::NetAddress& addr) {
  Log("StubUDP::Send(): size=%d", buf.size());
  Log("StubUDP::Send(): Pretending we received something.");
  AddPacket(nullptr);
  return buf.size();
}

int StubUDP::Bind(__attribute__((unused)) const pp::NetAddress& address) {
  Log("StubBind()");
  return 0;
}

}  // namespace PepperPOSIX
