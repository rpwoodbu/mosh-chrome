// pepper_posix_udp.cc - TCP Pepper POSIX adapters.
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

#include "pepper_posix_tcp.h"
#include <errno.h>
#include <stdio.h>

namespace PepperPOSIX {

Stream::Stream() { }

Stream::~Stream() { }

ssize_t Stream::Receive(void *buf, size_t count, int flags) {
  bool peek = false;
  if (flags & MSG_PEEK) {
    peek = true;
    flags &= ~MSG_PEEK;
  }
  if (flags != 0) {
    Log("Stream::Receive(): Unsupported flag: 0x%x", flags);
  }
  if (connection_errno_ != 0) {
    errno = ECONNABORTED;
    return -1;
  }

  pthread::MutexLock m(buffer_lock_);
  if (buffer_.size() == 0) {
    Log("Stream::Receive(): EWOULDBLOCK");
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

  return read_count;
}

ssize_t Stream::Read(void *buf, size_t count) {
  return Receive(buf, count, 0);
}

ssize_t Stream::Write(const void *buf, size_t count) {
  return Send(buf, count, 0);
}

void Stream::AddData(const void *buf, size_t count) {
  const char *cbuf = (const char *)buf;

  {
    pthread::MutexLock m(buffer_lock_);
    for (; count > 0; --count) {
      buffer_.push_back(*cbuf);
      ++cbuf;
    }
  }

  target_->UpdateRead(true);
}

int StubTCP::Bind(__attribute__((unused)) const pp::NetAddress &address) {
  Log("StubBind()");
  return 0;
}

int StubTCP::Connect(__attribute__((unused)) const pp::NetAddress &address) {
  Log("StubConnect()");
  return 0;
}

ssize_t StubTCP::Send(
    __attribute__((unused)) const void *buf,
    __attribute__((unused)) size_t count,
    __attribute__((unused)) int flags) {
  Log("StubSend()");
  return 0;
}

} // namespace PepperPOSIX
