// pepper_posix_udp.cc - UDP Pepper POSIX adapters.
//
// Pepper POSIX is a set of adapters to enable POSIX-like APIs to work with the
// callback-based APIs of Pepper (and transitively, JavaScript).

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

#include "pepper_posix_udp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace PepperPOSIX {

void DestroyMessage(struct ::msghdr *message) {
  free(message->msg_name);
  for (int i = 0; i < message->msg_iovlen; ++i) {
    free(message->msg_iov[i].iov_base);
    free(message->msg_iov + i);
  }
  assert(message->msg_control == NULL); // This isn't being used.

  delete message;
}

UDP::UDP() {
  pthread_mutex_init(&packets_lock_, NULL);
}

UDP::~UDP() {
  // There really shouldn't be another thread actively involved at destruction
  // time, but getting the lock nonetheless.
  pthread_mutex_lock(&packets_lock_);
  for (std::deque<struct ::msghdr *>::iterator i = packets_.begin();
      i != packets_.end();
      ++i) {
    DestroyMessage(*i);
  }
  pthread_mutex_unlock(&packets_lock_);
  pthread_mutex_destroy(&packets_lock_);
}

ssize_t UDP::Receive(struct ::msghdr *message, int flags) {
  pthread_mutex_lock(&packets_lock_);
  if (packets_.size() == 0) {
    pthread_mutex_unlock(&packets_lock_);
    errno = EWOULDBLOCK;
    return -1;
  }
  struct ::msghdr *latest = packets_.front();
  packets_.pop_front();
  target_->UpdateRead(packets_.size() > 0);
  pthread_mutex_unlock(&packets_lock_);

  if (message->msg_namelen >= latest->msg_namelen) {
    memcpy(message->msg_name, latest->msg_name, latest->msg_namelen);
  } else {
    Log("UDP::Receive(): msg_namelen too short.");
  }

  assert(latest->msg_iovlen == 1); // For simplicity, as this is internal.
  ssize_t size = 0;
  size_t input_len = latest->msg_iov->iov_len;
  for (int i = 0; i < message->msg_iovlen && size < input_len; ++i) {
    size_t output_len = message->msg_iov[i].iov_len;
    size_t to_copy = output_len <= input_len ? output_len : input_len;
    memcpy(message->msg_iov[i].iov_base, latest->msg_iov->iov_base, to_copy);
    size += to_copy;
  }
  assert(size == input_len); // TODO: Return a real error, or handle better.

  // TODO: Ignoring flags, msg_flags, and msg_control for now.

  DestroyMessage(latest);
  return size;
}

void UDP::AddPacket(struct ::msghdr *message) {
  pthread_mutex_lock(&packets_lock_);
  packets_.push_back(message);
  pthread_mutex_unlock(&packets_lock_);
  target_->UpdateRead(true);
}

ssize_t StubUDP::Send(
    const vector<char> &buf, int flags, const PP_NetAddress_IPv4 &addr) {
  Log("StubUDP::Send(): size=%d", buf.size());
  Log("StubUDP::Send(): Pretending we received something.");
  AddPacket(NULL);
  return buf.size();
}

int StubUDP::Bind(const PP_NetAddress_IPv4 &address) {
  Log("StubBind()");
  return 0;
}

} // namespace PepperPOSIX
