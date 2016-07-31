// pepper_posix.cc - Pepper POSIX adapters.
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

#include "pepper_posix.h"
#include "pepper_posix_native_tcp.h"
#include "pepper_posix_native_udp.h"
#include "pepper_posix_tcp.h"

#include "make_unique.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <memory>

namespace PepperPOSIX {

using std::move;
using std::string;
using std::unique_ptr;
using std::vector;
using util::make_unique;

const int SIGNAL_FD = -1;

POSIX::POSIX(const pp::InstanceHandle instance_handle,
             unique_ptr<Reader> std_in, unique_ptr<Writer> std_out,
             unique_ptr<Writer> std_err, unique_ptr<Signal> signal)
    : signal_(move(signal)), instance_handle_(instance_handle) {
  if (std_in != nullptr) {
    std_in->target_ = selector_.NewTarget(STDIN_FILENO);
  }
  files_[STDIN_FILENO] = move(std_in);
  if (std_out != nullptr) {
    std_out->target_ = selector_.NewTarget(STDOUT_FILENO);
    // Prevent buffering in stdout.
    assert(setvbuf(stdout, nullptr, _IONBF, 0) == 0);
  }
  files_[STDOUT_FILENO] = move(std_out);
  if (std_err != nullptr) {
    std_err->target_ = selector_.NewTarget(STDERR_FILENO);
    // Prevent buffering in stderr, but keep line mode as that works better for
    // keeping log lines together.
    assert(setvbuf(stderr, nullptr, _IOLBF, 0) == 0);
  }
  files_[STDERR_FILENO] = move(std_err);
  if (signal_ != nullptr) {
    // "Pseudo" file descriptor in Target needs to be set out of issuance
    // range.
    signal_->target_ = selector_.NewTarget(SIGNAL_FD);
  }
}

int POSIX::Open(const char* pathname, __attribute__((unused)) int flags,
                __attribute__((unused)) mode_t mode) {
  auto factories_iter = factories_.find(string(pathname));
  if (factories_iter == factories_.end()) {
    errno = EACCES;
    return -1;
  }
  // TODO: Error out if |file|'s type doesn't match |flags| (i.e., Reader
  // cannot be O_WRONLY).
  int fd = NextFileDescriptor();
  files_[fd] = factories_iter->second();
  files_[fd]->target_ = selector_.NewTarget(fd);
  return fd;
}

int POSIX::Close(int fd) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }

  int result = files_[fd]->Close();
  files_.erase(fd);

  return result;
}

ssize_t POSIX::Read(int fd, void* buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Reader* reader = dynamic_cast<Reader*>(files_[fd].get());
  if (reader == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (reader->IsBlocking()) {
    vector<Target *> read_targets, write_targets;
    read_targets.push_back(reader->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  return reader->Read(buf, count);
}

ssize_t POSIX::Write(int fd, const void* buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Writer* writer = dynamic_cast<Writer*>(files_[fd].get());
  if (writer == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (writer->IsBlocking()) {
    vector<Target *> read_targets, write_targets;
    write_targets.push_back(writer->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  return writer->Write(buf, count);
}

int POSIX::NextFileDescriptor() {
  for (int fd = 0;; ++fd) {
    if (files_.count(fd) == 0) {
      return fd;
    }
  }
}

int POSIX::Socket(int domain, int type, int protocol) {
  unique_ptr<File> file;

  if (domain == AF_UNIX && protocol == 0) {
    if (type == SOCK_STREAM && unix_socket_stream_factory_) {
      file = unix_socket_stream_factory_();
    }
  } else {
    if (domain != AF_INET && domain != AF_INET6) {
      errno = EINVAL;
      return -1;
    }
    if (type == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP)) {
      file = make_unique<NativeUDP>(instance_handle_);
    } else if (type == SOCK_STREAM &&
               (protocol == 0 || protocol == IPPROTO_TCP)) {
      file = make_unique<NativeTCP>(instance_handle_);
    }
  }

  if (file == nullptr) {
    errno = EINVAL;
    return -1;
  }

  int fd = NextFileDescriptor();
  file->target_ = selector_.NewTarget(fd);
  if (type == SOCK_STREAM) {
    // SOCK_STREAM should not be writable at first.
    file->target_->UpdateWrite(false);
  }
  files_[fd] = move(file);
  return fd;
}

int POSIX::Dup(int oldfd) {
  if (files_.count(oldfd) == 0) {
    errno = EBADF;
    return -1;
  }
  // Currently can only dup UDP sockets.
  UDP* udp = dynamic_cast<UDP*>(files_[oldfd].get());
  if (udp == nullptr) {
    errno = EBADF;
    return -1;
  }

  // NB: This socket implementation doesn't do anything with |domain|.
  return Socket(AF_INET, SOCK_DGRAM, 0);
}

int POSIX::PSelect(int nfds, fd_set* readfds, fd_set* writefds,
                   fd_set* exceptfds, const struct timespec* timeout,
                   __attribute__((unused)) const sigset_t* sigmask) {
  int result = 0;
  fd_set new_readfds, new_writefds;
  FD_ZERO(&new_readfds);
  FD_ZERO(&new_writefds);

  vector<Target *> read_targets, write_targets;
  for (int fd = 0; fd < nfds; ++fd) {
    if (readfds != nullptr && FD_ISSET(fd, readfds)) {
      read_targets.push_back(files_[fd]->target_.get());
    }
    if (writefds != nullptr && FD_ISSET(fd, writefds)) {
      write_targets.push_back(files_[fd]->target_.get());
    }
  }

  // Signal is handled specially.
  if (signal_ != nullptr) {
    read_targets.push_back(signal_->target_.get());
  }

  vector<Target*> ready_targets =
      selector_.Select(read_targets, write_targets, timeout);

  for (const auto* target : ready_targets) {
    const int fd = target->id();

    // Signal is handled specially.
    if (fd == SIGNAL_FD && signal_ != nullptr &&
        signal_->target_->has_read_data()) {
      signal_->Handle();
      continue;
    }

    if (readfds != nullptr && FD_ISSET(fd, readfds) &&
        target->has_read_data()) {
      FD_SET(fd, &new_readfds);
      ++result;
    }
    if (writefds != nullptr && FD_ISSET(fd, writefds) &&
        target->has_write_data()) {
      FD_SET(fd, &new_writefds);
      ++result;
    }
  }

  if (readfds != nullptr) {
    FD_ZERO(readfds);
  }
  if (writefds != nullptr) {
    FD_ZERO(writefds);
  }
  if (exceptfds != nullptr) {
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

int POSIX::Select(int nfds, fd_set* readfds, fd_set* writefds,
                  fd_set* exceptfds, struct timeval* timeout) {
  if (timeout != nullptr) {
    struct timespec ts;
    ts.tv_sec = timeout->tv_sec;
    ts.tv_nsec = timeout->tv_usec * 1000;
    return PSelect(nfds, readfds, writefds, exceptfds, &ts, nullptr);
  }
  return PSelect(nfds, readfds, writefds, exceptfds, nullptr, nullptr);
}

int POSIX::Poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  // Poll() is used infrequently, so just wrap PSelect(). This is an imperfect
  // implementation, but suffices.
  fd_set readfds, writefds, exceptfds;
  int pselect_nfds = 0;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  for (int i = 0; i < nfds; ++i) {
    const auto& fd = fds[i].fd;
    const auto& events = fds[i].events;

    if (events & (POLLIN | POLLPRI)) {
      FD_SET(fd, &readfds);
    }
    if (events & POLLOUT) {
      FD_SET(fd, &writefds);
    }
    if (events & (POLLERR | POLLHUP | POLLNVAL)) {
      FD_SET(fd, &exceptfds);
    }

    if (pselect_nfds <= fd) {
      pselect_nfds = fd + 1;
    }
  }

  struct timespec ts;
  // |timeout| is in milliseconds.
  ts.tv_sec = timeout / 1000;
  ts.tv_nsec = (timeout % 1000) * 1000000;

  int result =
      PSelect(pselect_nfds, &readfds, &writefds, &exceptfds, &ts, nullptr);

  for (int i = 0; i < nfds; ++i) {
    const auto& fd = fds[i].fd;
    auto& revents = fds[i].revents;

    // Cheating: Just setting every potentially related bit.
    if (FD_ISSET(fd, &readfds)) {
      revents |= POLLIN | POLLPRI;
    }
    if (FD_ISSET(fd, &writefds)) {
      revents |= POLLOUT;
    }
    if (FD_ISSET(fd, &exceptfds)) {
      revents |= POLLERR | POLLHUP | POLLNVAL;
    }
  }

  return result;
}

ssize_t POSIX::Recv(int sockfd, void* buf, size_t len, int flags) {
  if (files_.count(sockfd) == 0) {
    errno = EBADF;
    return -1;
  }
  TCP* tcp = dynamic_cast<TCP*>(files_[sockfd].get());
  if (tcp == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (tcp->IsBlocking() && !(flags & MSG_DONTWAIT)) {
    vector<Target *> read_targets, write_targets;
    read_targets.push_back(tcp->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  return tcp->Receive(buf, len, flags);
}

ssize_t POSIX::RecvMsg(int sockfd, struct msghdr* msg, int flags) {
  if (files_.count(sockfd) == 0) {
    errno = EBADF;
    return -1;
  }
  UDP* udp = dynamic_cast<UDP*>(files_[sockfd].get());
  if (udp == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (udp->IsBlocking() && !(flags & MSG_DONTWAIT)) {
    vector<Target *> read_targets, write_targets;
    read_targets.push_back(udp->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  return udp->Receive(msg, flags);
}

pp::NetAddress POSIX::MakeAddress(const struct sockaddr* addr,
                                  socklen_t addrlen) const {
  switch (addr->sa_family) {
    case AF_INET: {
      assert(addrlen >= sizeof(sockaddr_in));
      PP_NetAddress_IPv4 pp_addr;
      const struct sockaddr_in* in_addr = (struct sockaddr_in*)addr;
      uint32_t a = in_addr->sin_addr.s_addr;
      for (int i = 0; i < 4; ++i) {
        pp_addr.addr[i] = a & 0xff;
        a >>= 8;
      }
      pp_addr.port = in_addr->sin_port;
      return pp::NetAddress(instance_handle_, pp_addr);
    }

    case AF_INET6: {
      assert(addrlen >= sizeof(sockaddr_in6));
      PP_NetAddress_IPv6 pp_addr;
      const struct sockaddr_in6* in6_addr = (struct sockaddr_in6*)addr;
      memcpy(pp_addr.addr, in6_addr->sin6_addr.s6_addr, sizeof(pp_addr.addr));
      pp_addr.port = in6_addr->sin6_port;
      return pp::NetAddress(instance_handle_, pp_addr);
    }

    default:
      // Unsupported address family.
      assert(false);
      break;
  }

  // Should not get here.
  return pp::NetAddress();
}

ssize_t POSIX::Send(int sockfd, const void* buf, size_t len, int flags) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  TCP* tcp = dynamic_cast<TCP*>(files_[sockfd].get());
  if (tcp == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (tcp->IsBlocking() && !(flags & MSG_DONTWAIT)) {
    vector<Target *> read_targets, write_targets;
    write_targets.push_back(tcp->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  return tcp->Send(buf, len, flags);
}

ssize_t POSIX::SendTo(int sockfd, const void* buf, size_t len, int flags,
                      const struct sockaddr* dest_addr, socklen_t addrlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  UDP* udp = dynamic_cast<UDP*>(files_[sockfd].get());
  if (udp == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (udp->IsBlocking() && !(flags & MSG_DONTWAIT)) {
    vector<Target *> read_targets, write_targets;
    write_targets.push_back(udp->target_.get());
    selector_.Select(read_targets, write_targets, nullptr);
  }

  vector<char> buffer((const char*)buf, (const char*)buf + len);
  return udp->Send(move(buffer), flags, MakeAddress(dest_addr, addrlen));
}

int POSIX::FCntl(int fd, int cmd, va_list arg) {
  if (files_.count(fd) == 0) {
    return EBADF;
  }
  auto& file = files_[fd];

  if (cmd == F_SETFL) {
    bool blocking = true;
    long long_arg = va_arg(arg, long);
    if (long_arg & O_NONBLOCK) {
      blocking = false;
      long_arg &= ~O_NONBLOCK;
    }
    if (long_arg != 0) {
      Log("POSIX::FCntl(): Got F_SETFL, but unsupported arg: 0%lo", long_arg);
      // TODO: Consider this an error?
    }
    file->SetBlocking(blocking);
    return 0;
  }

  if (cmd == F_SETFD) {
    long long_arg = va_arg(arg, long);
    if (long_arg && FD_CLOEXEC) {
      // We don't support exec() anyway, so just ignore this.
      return 0;
    }
  }

  // Anything we don't explicitly handle or ignore is considered an error, to
  // avoid any potential confusion.
  Log("POSIX::FCntl(): Unsupported cmd/arg");
  errno = EINVAL;
  return -1;
}

int POSIX::Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  File* file = files_[sockfd].get();

  TCP* tcp = dynamic_cast<TCP*>(file);
  if (tcp != nullptr) {
    return tcp->Connect(MakeAddress(addr, addrlen));
  }

  UnixSocketStream* unix_socket = dynamic_cast<UnixSocketStream*>(file);
  if (unix_socket != nullptr) {
    const struct sockaddr_un* addr_un = (const struct sockaddr_un*)addr;
    if (addr_un->sun_family != AF_UNIX) {
      errno = EBADF;
      return -1;
    }
    return unix_socket->Connect(string(addr_un->sun_path));
  }

  errno = EBADF;
  return -1;
}

int POSIX::GetSockOpt(int sockfd, int level, int optname, void* optval,
                      socklen_t* optlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  TCP* tcp = dynamic_cast<TCP*>(files_[sockfd].get());
  if (tcp == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (optname == SO_ERROR && level == SOL_SOCKET) {
    // This allows nonblocking TCP connections to discover the disposition of a
    // connection attempt.
    if (*optlen < sizeof(tcp->connection_errno_)) {
      errno = EINVAL;
      return -1;
    }
    *(int*)optval = tcp->connection_errno_;
    return 0;
  }

  // No other options are currently implemented.
  Log("POSIX::GetSockOpt(): Unsupported optname/level");
  errno = EINVAL;
  return -1;
}

}  // namespace PepperPOSIX
