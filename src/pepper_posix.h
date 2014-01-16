// pepper_posix.h - Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX
#define PEPPER_POSIX

#include "pepper_posix_selector.h"

#include <map>
#include <string>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ppapi/c/ppb_net_address.h"
#include "ppapi/cpp/instance_handle.h"

using ::std::map;
using ::std::string;

// Implement this to plumb logging from Pepper functions to your app.
void Log(const char *format, ...);

namespace PepperPOSIX {

// Abstract class representing a POSIX file.
class File {
 public:
  File() : target_(NULL) {}
  virtual ~File() { delete target_; };

  virtual int Close() { return 0; }
  int fd() {
    if (target_ == NULL) {
      return -1;
    }
    return target_->id();
  }

  Target *target_;

 private:
  // Disable copy and assignment.
  File(const File &);
  File &operator=(const File &);
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
class ReadWriter : public Reader, public Writer {
};

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
  // Set any of these to NULL if not used. Takes ownership of all.
  POSIX(const pp::InstanceHandle &instance_handle,
      Reader *std_in, Writer *std_out, Writer *std_err, Signal *signal);
  ~POSIX();

  int Open(const char *pathname, int flags, mode_t mode);

  ssize_t Read(int fd, void *buf, size_t count);

  ssize_t Write(int fd, const void *buf, size_t count);

  int Close(int fd);

  int Socket(int domain, int type, int protocol);

  int Dup(int oldfd);

  int PSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
      const struct timespec *timeout, const sigset_t *sigmask);

  ssize_t Recv(int sockfd, void *buf, size_t len, int flags);

  ssize_t RecvMsg(int sockfd, struct msghdr *msg, int flags);

  ssize_t Send(int sockfd, const void *buf, size_t len, int flags);

  ssize_t SendTo(int sockfd, const void *buf, size_t len, int flags,
      const struct sockaddr *dest_addr, socklen_t addrlen);

  int FCntl(int fd, int cmd, va_list args);

  int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

  // Register a filename and File factory to be used when that file is
  // opened.
  void RegisterFile(string filename, File *(*factory)()) {
    factories_[filename] = factory;
  }

 private:
  // Returns the next available file descriptor.
  int NextFileDescriptor();

  // Map of file descriptors and the File objects they represent.
  map<int, File *> files_;
  // Map of registered files and their File factories.
  map<string, File *(*)()> factories_;
  Signal *signal_;
  Selector selector_;
  const pp::InstanceHandle &instance_handle_;

  // Disable copy and assignment.
  POSIX(const POSIX &);
  POSIX &operator=(const POSIX &);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX
