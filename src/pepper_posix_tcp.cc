// pepper_posix_udp.cc - TCP Pepper POSIX adapters.
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

#include "pepper_posix_tcp.h"
#include <errno.h>
#include <stdio.h>
#include <string.h> // DONOTSUBMIT

namespace PepperPOSIX {

TCP::TCP() {
  pthread_mutex_init(&buffer_lock_, NULL);
}

TCP::~TCP() {
  pthread_mutex_destroy(&buffer_lock_);
}

ssize_t TCP::Receive(void *buf, size_t count, int flags) {
  bool peek = false;
  if (flags & MSG_PEEK) {
    peek = true;
    flags &= ~MSG_PEEK;
  }
  if (flags != 0) {
    Log("TCP::Receive(): Unsupported flag: 0x%x", flags);
  }

  pthread_mutex_lock(&buffer_lock_);
  if (buffer_.size() == 0) {
    pthread_mutex_unlock(&buffer_lock_);
    Log("TCP::Receive(): EWOULDBLOCK");
    errno = EWOULDBLOCK;
    return -1;
  }
  char *cbuf = (char *)buf;
  int read_count = 0;
  if (peek) {
    for (;
        read_count < count && read_count < buffer_.size();
        ++read_count) {
      *cbuf = buffer_[read_count];
      ++cbuf;
    }
  } else {
    for (;
        read_count < count && buffer_.size() > 0;
        ++read_count) {
      *cbuf = buffer_.front();
      buffer_.pop_front();
      ++cbuf;
    }
    target_->UpdateRead(buffer_.size() > 0);
  }
  pthread_mutex_unlock(&buffer_lock_);

  return read_count;
}

ssize_t TCP::Read(void *buf, size_t count) {
  return Receive(buf, count, 0);
}

ssize_t TCP::Write(const void *buf, size_t count) {
  return Send(buf, count, 0);
}

void TCP::AddData(const void *buf, size_t count) {
  pthread_mutex_lock(&buffer_lock_);
  const char *cbuf = (const char *)buf;
  for (; count > 0; --count) {
    buffer_.push_back(*cbuf);
    ++cbuf;
  }
  pthread_mutex_unlock(&buffer_lock_);
  target_->UpdateRead(true);
}

int StubTCP::Bind(const PP_NetAddress_IPv4 &address) {
  Log("StubBind()");
  return 0;
}

int StubTCP::Connect(const PP_NetAddress_IPv4 &address) {
  Log("StubConnect()");
  return 0;
}

ssize_t StubTCP::Send(const void *buf, size_t count, int flags) {
  Log("StubSend()");
  return 0;
}

} // namespace PepperPOSIX
