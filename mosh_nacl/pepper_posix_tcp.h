// pepper_posix_udp.h - TCP Pepper POSIX adapters.
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

#ifndef MOSH_NACL_PEPPER_POSIX_TCP_H_
#define MOSH_NACL_PEPPER_POSIX_TCP_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <deque>
#include <string>
#include <vector>

#include "mosh_nacl/pepper_posix.h"
#include "mosh_nacl/pepper_posix_selector.h"
#include "mosh_nacl/pthread_locks.h"

#include "ppapi/cpp/net_address.h"

namespace PepperPOSIX {

// Stream implements the basic POSIX emulation logic for SOCK_STREAM
// communication. It is not fully implemented. An implementation should fully
// implement Send(), and insert received data using AddData(). It is expected
// that AddData() will be called from a different thread than the one calling
// other methods; no other thread safety is provided.
class Stream : public ReadWriter {
 public:
  Stream();
  ~Stream() override;

  // Read replaces read().
  ssize_t Read(void* buf, size_t count) override;

  // Recv replaces recv().
  virtual ssize_t Receive(void* buf, size_t count, int flags);

  // Write replaces write().
  ssize_t Write(const void* buf, size_t count) override;

  // Send replaces send().
  virtual ssize_t Send(const void* buf, size_t count, int flags) = 0;

  // Connection status, errno-style.
  int connection_errno_ = 0;

 protected:
  // AddData is used by the subclass to add data to the incoming buffer.
  // This method can be called from another thread than the one used to call
  // the other methods. Takes ownership of *message and its associated buffers.
  void AddData(const void* buf, size_t count);

 private:
  std::deque<char> buffer_;  // Guard with buffer_lock_.
  pthread::Mutex buffer_lock_;

  // Disable copy and assignment.
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
};

// TCP adds the TCP-specific interfaces to Stream. An implementation must also
// fully implement Bind() and Connect().
class TCP : public Stream {
 public:
  // Bind replaces bind().
  virtual int Bind(const pp::NetAddress& address) = 0;

  // Connect replaces connect().
  virtual int Connect(const pp::NetAddress& address) = 0;
};

// UnixSocketStream adds to Stream the interfaces specific to Unix domain
// sockets in SOCK_STREAM mode.
class UnixSocketStream : public Stream {
 public:
  // Bind replaces bind().
  virtual int Bind(const std::string& path) = 0;

  // Connect replaces connect().
  virtual int Connect(const std::string& path) = 0;
};

// StubTCP is an instantiatable stubbed subclass of TCP for debugging.
class StubTCP : public TCP {
 public:
  StubTCP() {}
  ~StubTCP() override {}

  ssize_t Send(const void* buf, size_t count, int flags) override;
  int Bind(const pp::NetAddress& address) override;
  int Connect(const pp::NetAddress& address) override;

 private:
  // Disable copy and assignment.
  StubTCP(const StubTCP&) = delete;
  StubTCP& operator=(const StubTCP&) = delete;
};

}  // namespace PepperPOSIX

#endif  // MOSH_NACL_PEPPER_POSIX_TCP_H_
