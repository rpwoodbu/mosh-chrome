// pepper_posix.cc - Pepper POSIX adapters.
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

#include "pepper_posix.h"
#include "pepper_posix_native_udp.h"
#include "pepper_posix_native_tcp.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>

namespace PepperPOSIX {

const int SIGNAL_FD = -1;

POSIX::POSIX(const pp::InstanceHandle &instance_handle,
    Reader *std_in, Writer *std_out, Writer *std_err, Signal *signal)
  : instance_handle_(instance_handle), signal_(signal) {
  files_[STDIN_FILENO] = std_in;
  if (std_in != NULL) {
    std_in->target_ = selector_.NewTarget(STDIN_FILENO);
  }
  files_[STDOUT_FILENO] = std_out;
  if (std_out != NULL) {
    std_out->target_ = selector_.NewTarget(STDOUT_FILENO);
  }
  files_[STDERR_FILENO] = std_err;
  if (std_err != NULL) {
    std_err->target_ = selector_.NewTarget(STDERR_FILENO);
  }
  if (signal_ != NULL) {
    // "Pseudo" file descriptor in Target needs to be set out of issuance
    // range.
    signal_->target_ = selector_.NewTarget(SIGNAL_FD);
  }
}

POSIX::~POSIX() {
  for (::std::map<int, File *>::iterator i = files_.begin();
      i != files_.end();
      ++i) {
    Close(i->first);
  }
}

int POSIX::Open(const char *pathname, int flags, mode_t mode) {
  ::std::map<string, File *(*)()>::iterator factories_iter =
      factories_.find(string(pathname));
  if (factories_iter == factories_.end()) {
    errno = EACCES;
    return -1;
  }
  File *file = factories_iter->second();
  // TODO: Error out if |file|'s type doesn't match |flags| (i.e., Reader
  // cannot be O_WRONLY).
  int fd = NextFileDescriptor();
  files_[fd] = file;
  file->target_ = selector_.NewTarget(fd);
  return fd;
}

int POSIX::Close(int fd) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }

  int result = files_[fd]->Close();
  delete files_[fd];
  files_.erase(fd);

  return result;
}

ssize_t POSIX::Read(int fd, void *buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Reader *reader = dynamic_cast<Reader *>(files_[fd]);
  if (reader == NULL) {
    errno = EBADF;
    return -1;
  }

  return reader->Read(buf, count);
}

ssize_t POSIX::Write(int fd, const void *buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Writer *writer = dynamic_cast<Writer *>(files_[fd]);
  if (writer == NULL) {
    errno = EBADF;
    return -1;
  }

  return writer->Write(buf, count);
}

int POSIX::NextFileDescriptor() {
  for (int fd = 0; ; ++fd) {
    if (files_.count(fd) == 0) {
      return fd;
    }
  }
}

int POSIX::Socket(int domain, int type, int protocol) {
  if (domain != AF_INET) {
    errno = EINVAL;
    return -1;
  }

  File *file = NULL;

  if (type == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP)) {
    file = new NativeUDP(instance_handle_);
  } else if (type == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP)) {
    file = new NativeTCP(instance_handle_);
  }

  if (file != NULL) {
    int fd = NextFileDescriptor();
    file->target_ = selector_.NewTarget(fd);
    files_[fd] = file;
    return fd;
  }

  errno = EINVAL;
  return -1;
}

int POSIX::Dup(int oldfd) {
  if (files_.count(oldfd) == 0) {
    errno = EBADF;
    return -1;
  }
  // Currently can only dup UDP sockets.
  UDP *udp = dynamic_cast<UDP *>(files_[oldfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  return Socket(AF_INET, SOCK_DGRAM, 0);
}

int POSIX::PSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const sigset_t *sigmask) {
  int result = 0;
  fd_set new_readfds, new_writefds;
  FD_ZERO(&new_readfds);
  FD_ZERO(&new_writefds);

  vector<Target *> read_targets, write_targets;
  for (int fd = 0; fd < nfds; ++fd) {
    if (readfds != NULL && FD_ISSET(fd, readfds)) {
      read_targets.push_back(files_[fd]->target_);
    }
    if (writefds != NULL && FD_ISSET(fd, writefds)) {
      write_targets.push_back(files_[fd]->target_);
    }
  }

  // Signal is handled specially.
  if (signal_ != NULL) {
    read_targets.push_back(signal_->target_);
  }

  vector<Target *> ready_targets = selector_.Select(
      read_targets, write_targets, timeout);

  for (vector<Target *>::iterator i = ready_targets.begin();
      i != ready_targets.end();
      ++i) {
    int fd = (*i)->id();

    // Signal is handled specially.
    if (fd == SIGNAL_FD && signal_ != NULL && signal_->target_->has_read_data()) {
      signal_->Handle();
      continue;
    }

    if (readfds != NULL && FD_ISSET(fd, readfds) && (*i)->has_read_data()) {
      FD_SET(fd, &new_readfds);
      ++result;
    }
    if (writefds != NULL && FD_ISSET(fd, writefds) && (*i)->has_write_data()) {
      FD_SET(fd, &new_writefds);
      ++result;
    }
  }

  if (readfds != NULL) {
    FD_ZERO(readfds);
  }
  if (writefds != NULL) {
    FD_ZERO(writefds);
  }
  if (exceptfds != NULL) {
    FD_ZERO(exceptfds);
  }
  for (int fd = 0; fd < nfds; ++fd) {
    if (FD_ISSET(fd, &new_readfds)) {
      FD_SET(fd, readfds);
    }
    if (FD_ISSET(fd, &new_writefds)) {
      FD_SET(fd, writefds);
    }
  }

  return result;
}

ssize_t POSIX::Recv(int sockfd, void *buf, size_t len, int flags) {
  if (files_.count(sockfd) == 0) {
    errno = EBADF;
    return -1;
  }
  TCP *tcp = dynamic_cast<TCP *>(files_[sockfd]);
  if (tcp == NULL) {
    errno = EBADF;
    return -1;
  }

  return tcp->Receive(buf, len, flags);
}

ssize_t POSIX::RecvMsg(int sockfd, struct msghdr *msg, int flags) {
  if (files_.count(sockfd) == 0) {
    errno = EBADF;
    return -1;
  }
  UDP *udp = dynamic_cast<UDP *>(files_[sockfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  return udp->Receive(msg, flags);
}

// Make a PP_NetAddress_IPv4 from sockaddr.
void MakeAddress(const struct sockaddr *addr, socklen_t addrlen,
    PP_NetAddress_IPv4 *pp_addr) {
  // TODO: Make an IPv6 version, but since mosh doesn't support it now, this
  // will do.
  assert(addr->sa_family == AF_INET);
  assert(addrlen >= 4);
  const struct sockaddr_in *in_addr = (struct sockaddr_in*)addr;
  uint32_t a = in_addr->sin_addr.s_addr;
  for (int i = 0; i < 4; ++i) {
    pp_addr->addr[i] = a & 0xff;
    a >>= 8;
  }
  pp_addr->port = in_addr->sin_port;
}

ssize_t POSIX::Send(int sockfd, const void *buf, size_t len, int flags) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  TCP *tcp = dynamic_cast<TCP *>(files_[sockfd]);
  if (tcp == NULL) {
    errno = EBADF;
    return -1;
  }

  return tcp->Send(buf, len, flags);
}

ssize_t POSIX::SendTo(int sockfd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  UDP *udp = dynamic_cast<UDP *>(files_[sockfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  vector<char> buffer((const char*)buf, (const char*)buf+len);
  PP_NetAddress_IPv4 addr;
  MakeAddress(dest_addr, addrlen, &addr);

  return udp->Send(buffer, flags, addr);
}

int POSIX::FCntl(int fd, int cmd, va_list arg) {
  if (cmd == F_SETFL) {
    long long_arg = va_arg(arg, long);
    if (long_arg & O_NONBLOCK) {
      // For now, everything is nonblocking, so this is a no-op.
      return 0;
    }
    Log("POSIX::FCntl(): Got F_SETFL, but unsupported arg: 0%lo", long_arg);
    // TODO: Consider this an error?
    return 0;
  }

  // Anything we don't explicitly handle or ignore is considered an error, to
  // avoid any potential confusion.
  Log("POSIX::FCntl(): Unsupported cmd/arg");
  errno = EINVAL;
  return -1;
}

int POSIX::Connect(
    int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  TCP *tcp = dynamic_cast<TCP *>(files_[sockfd]);
  if (tcp == NULL) {
    errno = EBADF;
    return -1;
  }

  PP_NetAddress_IPv4 pp_addr;
  MakeAddress(addr, addrlen, &pp_addr);

  return tcp->Connect(pp_addr);
}

} // namespace PepperPOSIX
