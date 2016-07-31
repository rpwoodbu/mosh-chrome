// pepper_posix_udp.h - UDP Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX_UDP_HPP
#define PEPPER_POSIX_UDP_HPP

#include "pepper_posix.h"
#include "pepper_posix_selector.h"
#include "pthread_locks.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <deque>
#include <memory>
#include <vector>

#include "ppapi/cpp/net_address.h"

namespace PepperPOSIX {

// Wrapper over struct msghdr, which ensures proper destruction.
struct MsgHdr : public ::msghdr {
  MsgHdr() = delete;
  MsgHdr(const pp::NetAddress& addr, int32_t size, const char* const buf);
  ~MsgHdr();

  // Disable copy and assignment.
  MsgHdr(const MsgHdr&) = delete;
  MsgHdr& operator=(const MsgHdr&) = delete;
};

// UDP implements the basic POSIX emulation logic for UDP communication. It is
// not fully implemented. An implementation should fully implement Bind() and
// Send(), and insert received packets using AddPacket(). It is expected that
// AddPacket() will be called from a different thread than the one calling the
// other methods; no other thread safety is provided.
class UDP : public File {
 public:
  UDP();
  ~UDP() override;

  // Receive replaces recvmsg(); see its documentation for usage.
  ssize_t Receive(struct ::msghdr* message, int flags);

  // Bind replaces bind().
  virtual int Bind(const pp::NetAddress& address) = 0;

  // Send replaces sendto(). Usage is similar, but tweaked for C++.
  virtual ssize_t Send(const std::vector<char>& buf, int flags,
                       const pp::NetAddress& address) = 0;

 protected:
  // AddPacket is used by the subclass to add a packet to the incoming queue.
  // This method can be called from another thread than the one used to call
  // the other methods. Takes ownership of *message and its associated buffers.
  void AddPacket(std::unique_ptr<MsgHdr> message);

 private:
  std::deque<std::unique_ptr<MsgHdr>> packets_;  // Guard with packets_lock_.
  pthread::Mutex packets_lock_;

  // Disable copy and assignment.
  UDP(const UDP&) = delete;
  UDP& operator=(const UDP&) = delete;
};

// StubUDP is an instantiatable stubbed subclass of UDP for debugging.
class StubUDP : public UDP {
 public:
  // Bind replaces bind().
  int Bind(const pp::NetAddress& address) override;

  // Send replaces sendto. Usage is similar, but tweaked for C++.
  ssize_t Send(const std::vector<char>& buf, int flags,
               const pp::NetAddress& address) override;

 private:
  // Disable copy and assignment.
  StubUDP(const StubUDP&) = delete;
  StubUDP& operator=(const StubUDP&) = delete;
};

}  // namespace PepperPOSIX

#endif  // PEPPER_POSIX_UDP_HPP
