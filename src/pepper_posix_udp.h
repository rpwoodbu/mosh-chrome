// pepper_posix_udp.h - UDP Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX_UDP_HPP
#define PEPPER_POSIX_UDP_HPP

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

// UDP implements the basic POSIX emulation logic for UDP communication. It is
// not fully implemented. An implementation should fully implement Bind() and
// Send(), and insert received packets using AddPacket(). It is expected that
// AddPacket() will be called from a different thread than the one calling the
// other methods; no other thread safety is provided.
class UDP : public File {
 public:
  UDP();
  virtual ~UDP();

  // Receive replaces recvmsg(); see its documentation for usage.
  ssize_t Receive(struct ::msghdr *message, int flags);

  // Bind replaces bind().
  virtual int Bind(const PP_NetAddress_IPv4 &address) = 0;

  // Send replaces sendto(). Usage is similar, but tweaked for C++.
  virtual ssize_t Send(
    const vector<char> &buf, int flags,
    const PP_NetAddress_IPv4 &address) = 0;

 protected:
  // AddPacket is used by the subclass to add a packet to the incoming queue.
  // This method can be called from another thread than the one used to call
  // the other methods. Takes ownership of *message and its associated buffers.
  void AddPacket(struct ::msghdr *message);
 
 private:
  std::deque<struct ::msghdr *> packets_; // Guard with packets_lock_.
  pthread_mutex_t packets_lock_;

  // Disable copy and assignment.
  UDP(const UDP &);
  UDP &operator=(const UDP &);
};

// StubUDP is an instantiatable stubbed subclass of UDP for debugging.
class StubUDP : public UDP {
 public:
  // Bind replaces bind().
  virtual int Bind(const PP_NetAddress_IPv4 &address);

  // Send replaces sendto. Usage is similar, but tweaked for C++.
  virtual ssize_t Send(
    const vector<char> &buf, int flags,
    const PP_NetAddress_IPv4 &address);

 private:
  // Disable copy and assignment.
  StubUDP(const StubUDP &);
  StubUDP &operator=(const StubUDP &);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX_UDP_HPP
