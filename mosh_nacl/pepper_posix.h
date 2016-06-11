// pepper_posix.h - Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX
#define PEPPER_POSIX

#include "pepper_posix_selector.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <poll.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ppapi/c/ppb_net_address.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/net_address.h"

// Implement this to plumb logging from Pepper functions to your app.
void Log(const char *format, ...);

namespace PepperPOSIX {

// Abstract class representing a POSIX file.
class File {
 public:
  File() {}
  virtual ~File() { Close(); }

  virtual int Close() { return 0; }
  int fd() {
    if (target_ == nullptr) {
      return -1;
    }
    return target_->id();
  }

  virtual const bool IsBlocking() { return blocking_; }
  virtual void SetBlocking(bool mode) { blocking_ = mode; }

 protected:
  friend class POSIX;
  std::unique_ptr<Target> target_;

 private:
  bool blocking_ = true;

  // Disable copy and assignment.
  File(const File &) = delete;
  File &operator=(const File &) = delete;
};

// Abstract class defining a file that is read-only.
class Reader : public virtual File {
 public:
  virtual ssize_t Read(void *buf, size_t count) = 0;
};

// Abstract class defining a file that is write-only.
class Writer : public virtual File {
 public:
  virtual ssize_t Write(const void *buf, size_t count) = 0;
};

// Abstract class defining a file that is read/write.
class ReadWriter : public Reader, public Writer {};

// Special File to handle signals. Write a method in your implementation that
// calls target_->UpdateRead(true) when there's an outstanding signal. Handle()
// will get called when there is.
class Signal : public File {
 public:
  // Implement this to handle a signal. It will be called from PSelect. It is
  // the responsibility of the implementer to track what signals are
  // outstanding. Call target_->UpdateRead(false) from this method when there
  // are no more outstanding signals.
  virtual void Handle() = 0;
};

// POSIX implements the top-level POSIX file functionality, allowing easy
// binding to "unistd.h" functions. As such, the methods bear a great
// resemblance to those functions.
class POSIX {
 public:
  // Provide implementations of Reader and Writer which emulate STDIN, STDOUT,
  // and STDERR. Provide an implementation of Signal to handle signals, which
  // will be called from PSelect().
  //
  // Set any of these to the default unique_ptr (nullptr) if not used.
  POSIX(
      const pp::InstanceHandle instance_handle,
      std::unique_ptr<Reader> std_in,
      std::unique_ptr<Writer> std_out,
      std::unique_ptr<Writer> std_err,
      std::unique_ptr<Signal> signal);

  ~POSIX() {};

  int Open(const char *pathname, int flags, mode_t mode);

  ssize_t Read(int fd, void *buf, size_t count);

  ssize_t Write(int fd, const void *buf, size_t count);

  int Close(int fd);

  int Socket(int domain, int type, int protocol);

  int Dup(int oldfd);

  int PSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
      const struct timespec *timeout, const sigset_t *sigmask);

  int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
      struct timeval *timeout);

  int Poll(struct pollfd *fds, nfds_t nfds, int timeout);

  ssize_t Recv(int sockfd, void *buf, size_t len, int flags);

  ssize_t RecvMsg(int sockfd, struct msghdr *msg, int flags);

  ssize_t Send(int sockfd, const void *buf, size_t len, int flags);

  ssize_t SendTo(int sockfd, const void *buf, size_t len, int flags,
      const struct sockaddr *dest_addr, socklen_t addrlen);

  int FCntl(int fd, int cmd, va_list args);

  int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

  int GetSockOpt(int sockfd, int level, int optname,
      void *optval, socklen_t *optlen);

  // Register a filename and File factory to be used when that file is
  // opened.
  void RegisterFile(
      std::string filename, std::function<std::unique_ptr<File> ()> factory) {
    factories_[filename] = factory;
  }

  // Register a File factory to be called every time a Unix domain socket of
  // type SOCK_STREAM is created by calling Socket().
  void RegisterUnixSocketStream(
      std::function<std::unique_ptr<File> ()> factory) {
    unix_socket_stream_factory_ = factory;
  }

 private:
  // Returns the next available file descriptor.
  int NextFileDescriptor();

  // Makes a pp::NetAddress from a sockaddr.
  pp::NetAddress MakeAddress(
    const struct sockaddr *addr, socklen_t addrlen) const;

  // Map of file descriptors and the File objects they represent.
  std::map<int, std::unique_ptr<File>> files_;
  // Map of registered files and their File factories.
  std::map<std::string, std::function<std::unique_ptr<File> ()>> factories_;
  // Factory function for creating Unix domain sockets of type SOCK_STREAM.
  std::function<std::unique_ptr<File> ()> unix_socket_stream_factory_;
  std::unique_ptr<Signal> signal_;
  Selector selector_;
  const pp::InstanceHandle instance_handle_;

  // Disable copy and assignment.
  POSIX(const POSIX &) = delete;
  POSIX &operator=(const POSIX &) = delete;
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX
