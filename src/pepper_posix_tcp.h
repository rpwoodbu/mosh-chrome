// pepper_posix_udp.h - TCP Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX_TCP_HPP
#define PEPPER_POSIX_TCP_HPP

#include "pepper_posix.h"
#include "pepper_posix_selector.h"

#include <deque>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "ppapi/c/ppb_net_address.h"

namespace PepperPOSIX {

// TCP implements the basic POSIX emulation logic for TCP communication. It is
// not fully implemented. An implementation should fully implement Bind(),
// Connect(), and Send(), and insert received data using AddData(). It is
// expected that AddData() will be called from a different thread than the one
// calling other methods; no other thread safety is provided.
class TCP : public ReadWriter {
 public:
  TCP();
  virtual ~TCP();

  // Read replaces read().
  virtual ssize_t Read(void *buf, size_t count);

  // Recv replaces recv().
  virtual ssize_t Receive(void *buf, size_t count, int flags);

  // Write replaces write().
  virtual ssize_t Write(const void *buf, size_t count);

  // Send replaces send().
  virtual ssize_t Send(const void *buf, size_t count, int flags) = 0;

  // Bind replaces bind().
  virtual int Bind(const PP_NetAddress_IPv4 &address) = 0;

  // Connect replaces connect().
  virtual int Connect(const PP_NetAddress_IPv4 &address) = 0;

 protected:
  // AddData is used by the subclass to add data to the incoming buffer.
  // This method can be called from another thread than the one used to call
  // the other methods. Takes ownership of *message and its associated buffers.
  void AddData(const void *buf, size_t count);
 
 private:
  std::deque<char> buffer_; // Guard with buffer_lock_.
  pthread_mutex_t buffer_lock_;

  // Disable copy and assignment.
  TCP(const TCP &);
  TCP &operator=(const TCP &);
};

// StubTCP is an instantiatable stubbed subclass of TCP for debugging.
class StubTCP : public TCP {
 public:
  StubTCP() {};
  virtual ~StubTCP() {};

  virtual ssize_t Send(const void *buf, size_t count, int flags);
  virtual int Bind(const PP_NetAddress_IPv4 &address);
  virtual int Connect(const PP_NetAddress_IPv4 &address);
 private:
  // Disable copy and assignment.
  StubTCP(const StubTCP &);
  StubTCP &operator=(const StubTCP &);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX_TCP_HPP
