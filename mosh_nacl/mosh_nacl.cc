// mosh_nacl.cc - Mosh for Native Client (NaCl).
//
// This file contains the Mosh-specific bits of the NaCl port of the
// client.

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

#include "mosh_nacl/mosh_nacl.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "mosh_nacl/gpdns_resolver.h"
#include "mosh_nacl/make_unique.h"
#include "mosh_nacl/pepper_posix_tcp.h"
#include "mosh_nacl/pepper_resolver.h"
#include "mosh_nacl/pthread_locks.h"

#include "irt.h"  // NOLINT(build/include)
#include "ppapi/cpp/module.h"

using std::back_inserter;
using std::copy;
using std::deque;
using std::function;
using std::map;
using std::move;
using std::unique_ptr;
using std::vector;
using util::make_unique;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char* argv[]);

// Used by pepper_wrapper.h functions.
static class MoshClientInstance* instance = nullptr;

// Implements most of the plumbing to get keystrokes to Mosh. A tiny amount of
// plumbing is in the MoshClientInstance::HandleMessage().
class Keyboard : public PepperPOSIX::Reader {
 public:
  Keyboard() = default;
  ~Keyboard() override = default;

  ssize_t Read(void* buf, size_t count) override {
    int num_read = 0;

    pthread::MutexLock m(keypresses_lock_);

    while (keypresses_.size() > 0 && num_read < count) {
      reinterpret_cast<char*>(buf)[num_read] = keypresses_.front();
      keypresses_.pop_front();
      ++num_read;
    }

    target_->UpdateRead(keypresses_.size() > 0);

    return num_read;
  }

  // Handle input from the keyboard.
  void HandleInput(const string& input) {
    if (input.size() == 0) {
      // Nothing to see here.
      return;
    }
    {
      pthread::MutexLock m(keypresses_lock_);
      for (int i = 0; i < input.size(); ++i) {
        keypresses_.push_back(input[i]);
      }
    }
    target_->UpdateRead(true);
  }

 private:
  // Queue of keyboard keypresses.
  deque<char> keypresses_;  // Guard with keypresses_lock_.
  pthread::Mutex keypresses_lock_;
};

// Implements the plumbing to get stdout to the terminal.
class Terminal : public PepperPOSIX::Writer {
 public:
  Terminal(MoshClientInstance& instance) : instance_(instance) {}
  ~Terminal() override = default;

  // This has to be defined below MoshClientInstance due to dependence on it.
  ssize_t Write(const void* buf, size_t count) override;

 private:
  MoshClientInstance& instance_;
};

// Implements the plumbing to get stderr to Javascript.
class ErrorLog : public PepperPOSIX::Writer {
 public:
  ErrorLog(MoshClientInstance& instance) : instance_(instance) {}
  ~ErrorLog() override = default;

  // This has to be defined below MoshClientInstance due to dependence on it.
  ssize_t Write(const void* buf, size_t count) override;

 private:
  MoshClientInstance& instance_;
};

// Implements the plumbing to get SIGWINCH to Mosh. A tiny amount of plumbing
// is in MoshClientInstance::HandleMessage();
class WindowChange : public PepperPOSIX::Signal {
 public:
  WindowChange() = default;
  ~WindowChange() override = default;

  // Update geometry and send SIGWINCH.
  void Update(int width, int height) {
    if (sigwinch_handler_ != nullptr) {
      width_ = width;
      height_ = height;
      target_->UpdateRead(true);
    }
  }

  void SetHandler(function<void(int)> sigwinch_handler) {
    sigwinch_handler_ = sigwinch_handler;
  }

  void Handle() override {
    if (sigwinch_handler_ != nullptr) {
      sigwinch_handler_(SIGWINCH);
      target_->UpdateRead(false);
    }
  }

  int height() const { return height_; }
  int width() const { return width_; }

 private:
  int width_ = 80;
  int height_ = 24;
  function<void(int)> sigwinch_handler_;
};

class DevURandom : public PepperPOSIX::Reader {
 public:
  DevURandom() {
    nacl_interface_query(NACL_IRT_RANDOM_v0_1, &random_, sizeof(random_));
  }
  ~DevURandom() override = default;

  ssize_t Read(void* buf, size_t count) override {
    size_t bytes_read = 0;
    random_.get_random_bytes(buf, count, &bytes_read);
    return bytes_read;
  }

 private:
  struct nacl_irt_random random_;
};

// Packetizer for SSH agent communications.
class SSHAgentPacketizer {
 public:
  SSHAgentPacketizer() = default;
  SSHAgentPacketizer(const SSHAgentPacketizer&) = delete;
  SSHAgentPacketizer& operator=(const SSHAgentPacketizer&) = delete;

  // Add data to the packetizer buffer.
  void AddData(string data) {
    copy(data.begin(), data.end(), back_inserter(buf_));
  }

  // Checks to see if ConsumePacket() will return a full packet.
  bool IsPacketAvailable() const {
    return buf_.size() >= GetSize() + kHeaderSize_;
  }

  // Returns one full packet, or an empty vector if one is not available.
  pp::VarArray ConsumePacket() {
    pp::VarArray result;

    if (!IsPacketAvailable()) {
      return result;  // Empty string (RVO).
    }

    const auto size = GetSize();
    for (int i = 0; i < size; ++i) {
      result.Set(i, pp::Var(buf_.at(i + kHeaderSize_)));
    }
    buf_.erase(buf_.begin(), buf_.begin() + kHeaderSize_ + size);
    return result;
  }

  // Generate a packet with size header from a pp::VarArray.
  static vector<uint8_t> PacketFromArray(const pp::VarArray& data) {
    vector<uint8_t> v_data;
    uint32_t size = data.GetLength();
    v_data.reserve(size + kHeaderSize_);

    v_data.push_back((size >> 24) & 0xff);
    v_data.push_back((size >> 16) & 0xff);
    v_data.push_back((size >> 8) & 0xff);
    v_data.push_back(size & 0xff);

    for (int i = 0; i < size; ++i) {
      v_data.push_back(static_cast<uint8_t>(data.Get(i).AsInt()));
    }
    return v_data;
  }

 private:
  // Get the size header value from the buffered packet. Returns zero if the
  // buffer does not contain enough data for the size header.
  uint32_t GetSize() const {
    if (buf_.size() < kHeaderSize_) {
      return 0;
    }

    return (static_cast<uint32_t>(buf_[0]) << 24) +
           (static_cast<uint32_t>(buf_[1]) << 16) +
           (static_cast<uint32_t>(buf_[2]) << 8) +
           (static_cast<uint32_t>(buf_[3]));
  }

  static const int kHeaderSize_ = 4;
  deque<uint8_t> buf_;
};

// Implements virtual Unix domain sockets, which is used to connect libssh to
// an ssh-agent.
class UnixSocketStreamImpl : public PepperPOSIX::UnixSocketStream {
 public:
  UnixSocketStreamImpl() = default;
  UnixSocketStreamImpl(MoshClientInstance& instance) : instance_(instance) {}
  ~UnixSocketStreamImpl() override {
    if (file_type_ == FileType::SSH_AUTH_SOCK) {
      instance_.set_ssh_agent_socket(nullptr);
    }
  }

  ssize_t Send(const void* buf, size_t count,
               __attribute__((unused)) int flags) override {
    switch (file_type_) {
      case FileType::UNSET:
        Log("UnixSocketStreamImpl::Send(): "
            "Attempted to send to unconnected socket.");
        errno = ENOTCONN;
        return -1;

      case FileType::SSH_AUTH_SOCK: {
        const char* bytes = static_cast<const char*>(buf);
        agent_packetizer_.AddData(string(bytes, count));
        if (agent_packetizer_.IsPacketAvailable()) {
          auto packet = agent_packetizer_.ConsumePacket();
          instance_.Output(MoshClientInstance::TYPE_SSH_AGENT, packet);
        }
        return count;
      }

      default:;  // Fallthrough.
    }

    Log("UnixSocketStreamImpl::Send(): Unhandled file type.");
    errno = EBADF;
    return -1;
  }

  int Connect(const string& path) override {
    if (file_type_ != FileType::UNSET) {
      Log("UnixSocketStreamImpl::Connect(): Already connected.");
      errno = EISCONN;
      return -1;
    }
    if (names_to_file_types_.count(path) == 0) {
      Log("UnixSocketStreamImpl::Connect(): Filename %s unsupported.",
          path.c_str());
      errno = EACCES;
      return -1;
    }
    // As a cheap hack to support blocking mode, valid calls to Connect()
    // always "succeed". The connection will already be established in
    // JavaScript, or agent support will be disabled.
    file_type_ = names_to_file_types_.at(path);
    if (file_type_ == FileType::SSH_AUTH_SOCK) {
      instance_.set_ssh_agent_socket(this);
    }
    target_->UpdateWrite(true);
    return 0;
  }

  int Bind(__attribute__((unused)) const string& path) override {
    // Not implemented.
    errno = EACCES;
    return -1;
  }

  void HandleInput(const pp::VarArray& data) {
    vector<uint8_t> v_data = SSHAgentPacketizer::PacketFromArray(data);
    AddData(v_data.data(), v_data.size());
  }

 private:
  enum class FileType {
    UNSET,
    SSH_AUTH_SOCK,
  };

  static const map<string, FileType> names_to_file_types_;
  FileType file_type_ = FileType::UNSET;
  SSHAgentPacketizer agent_packetizer_;
  MoshClientInstance& instance_;
};

const map<string, UnixSocketStreamImpl::FileType>
    UnixSocketStreamImpl::names_to_file_types_ = {
        {"agent", UnixSocketStreamImpl::FileType::SSH_AUTH_SOCK},
};

MoshClientInstance::MoshClientInstance(PP_Instance instance)
    : pp::Instance(instance), cc_factory_(this) {
  ++num_instances_;
  assert(num_instances_ == 1);
  ::instance = this;
}

MoshClientInstance::~MoshClientInstance() {
  // Wait for thread to finish.
  if (thread_) {
    pthread_join(thread_, nullptr);
  }
}

void MoshClientInstance::HandleMessage(const pp::Var& var) {
  if (var.is_dictionary() == false) {
    Log("HandleMessage(): Not a dictionary.");
    return;
  }

  pp::VarDictionary dict(var);

  if (dict.HasKey("keyboard")) {
    string s = dict.Get("keyboard").AsString();
    keyboard_->HandleInput(s);
  } else if (dict.HasKey("window_change")) {
    int32_t num = dict.Get("window_change").AsInt();
    window_change_->Update(num >> 16, num & 0xffff);
  } else if (dict.HasKey("ssh_key")) {
    pp::Var key = dict.Get("ssh_key");
    if (key.is_undefined() == false) {
      ssh_login_.set_key(key.AsString());
    }
    // Now get the known hosts.
    Output(TYPE_GET_KNOWN_HOSTS, "");
  } else if (dict.HasKey("known_hosts")) {
    pp::Var known_hosts = dict.Get("known_hosts");
    if (known_hosts.is_undefined() == false) {
      ssh_login_.set_known_hosts(known_hosts);
    }
    // The assumption is that Output(TYPE_GET_SSH_KEY, "") was already
    // called, which precipitated Output(TYPE_GET_KNOWN_HOSTS, ""), so now we
    // are ready to do the SSH login.
    LaunchSSHLogin();
  } else if (dict.HasKey("ssh_agent")) {
    if (ssh_agent_socket_ != nullptr) {
      ssh_agent_socket_->HandleInput(pp::VarArray(dict.Get("ssh_agent")));
    }
  } else {
    Log("HandleMessage(): Got a message of an unexpected type.");
  }
}

void MoshClientInstance::Output(OutputType t, const pp::Var& data) {
  string type;
  switch (t) {
    case TYPE_DISPLAY:
      type = "display";
      break;
    case TYPE_LOG:
      type = "log";
      break;
    case TYPE_ERROR:
      type = "error";
      break;
    case TYPE_GET_SSH_KEY:
      type = "get_ssh_key";
      break;
    case TYPE_GET_KNOWN_HOSTS:
      type = "sync_get_known_hosts";
      break;
    case TYPE_SET_KNOWN_HOSTS:
      type = "sync_set_known_hosts";
      break;
    case TYPE_SSH_AGENT:
      type = "ssh-agent";
      break;
    case TYPE_EXIT:
      type = "exit";
      break;
    default:
      // Bad type.
      return;
  }

  pp::VarDictionary dict;
  dict.Set("type", type);
  dict.Set("data", data);
  PostMessage(dict);
}

void MoshClientInstance::Logv(OutputType t, const string& format,
                              va_list argp) {
  char buf[1024];
  int size = vsnprintf(buf, sizeof(buf), format.c_str(), argp);
  Output(t, string((const char*)buf, size));
}

void MoshClientInstance::Log(const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  Logv(TYPE_LOG, format, argp);
  va_end(argp);
}

void MoshClientInstance::Error(const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  Logv(TYPE_ERROR, format, argp);
  va_end(argp);
}

bool MoshClientInstance::Init(uint32_t argc, const char* argn[],
                              const char* argv[]) {
  const char* secret = nullptr;
  string mosh_escape_key;
  for (int i = 0; i < argc; ++i) {
    string name = argn[i];
    int len = strlen(argv[i]) + 1;
    if (name == "key") {
      secret = argv[i];
    } else if (name == "addr" && addr_ == nullptr) {
      host_ = argv[i];
    } else if (name == "port" && port_ == nullptr) {
      port_ = make_unique<char[]>(len);
      strncpy(port_.get(), argv[i], len);
    } else if (name == "family") {
      const string family = argv[i];
      if (family == "IPv4") {
        type_ = Resolver::Type::A;
      } else if (family == "IPv6") {
        type_ = Resolver::Type::AAAA;
      }
    } else if (name == "mode") {
      if (string(argv[i]) == "ssh") {
        ssh_mode_ = true;
      }
    } else if (name == "user") {
      ssh_login_.set_user(argv[i]);
    } else if (name == "remote-command") {
      ssh_login_.set_remote_command(argv[i]);
    } else if (name == "server-command") {
      ssh_login_.set_server_command(argv[i]);
    } else if (name == "use-agent") {
      ssh_login_.set_use_agent(string(argv[i]) == "true");
    } else if (name == "mosh-escape-key") {
      mosh_escape_key = argv[i];
    } else if (name == "dns-resolver") {
      const string resolver_name = argv[i];
      if (resolver_name == "google-public-dns") {
        resolver_ = make_unique<GPDNSResolver>(this);
      } else {
        Log("Unknown resolver '%s'.", resolver_name.c_str());
        return false;
      }
    } else if (name == "trust-sshfp") {
      if (string(argv[i]) == "true") {
        ssh_login_.set_trust_sshfp(true);
      }
    }
  }

  if (host_.size() == 0 || port_ == nullptr) {
    Log("Must supply addr and port attributes.");
    return false;
  }

  if (ssh_mode_) {
    if (ssh_login_.user().size() == 0) {
      Log("Must provide a username for ssh mode.");
      return false;
    }
  } else {
    setenv("MOSH_KEY", secret, 1);
  }

  if (!mosh_escape_key.empty()) {
    setenv("MOSH_ESCAPE_KEY", mosh_escape_key.c_str(), 1);
  }

  // Setup communications. We keep pointers to |keyboard_| and
  // |window_change_|, as we need to access their specialized methods. |posix_|
  // owns them, but we own |posix_|, so it is all good so long as these "files"
  // are not closed.
  auto keyboard = make_unique<Keyboard>();
  auto window_change = make_unique<WindowChange>();
  keyboard_ = keyboard.get();
  window_change_ = window_change.get();
  posix_ = make_unique<PepperPOSIX::POSIX>(
      this, move(keyboard), make_unique<Terminal>(*this),
      make_unique<ErrorLog>(*this), move(window_change));
  posix_->RegisterFile("/dev/urandom",
                       []() { return make_unique<DevURandom>(); });
  posix_->RegisterUnixSocketStream(
      [this]() { return make_unique<UnixSocketStreamImpl>(*this); });

  if (resolver_ == nullptr) {
    // Use default resolver.
    resolver_ = make_unique<PepperResolver>(this);
  }

  if (ssh_mode_) {
    // HandleMessage() will call LaunchSSHLogin().
    Output(TYPE_GET_SSH_KEY, "");
  } else {
    // Mosh will launch via this callback when the resolution completes.
    resolver_->Resolve(host_, type_, [this](Resolver::Error error,
                                            Resolver::Authenticity authenticity,
                                            vector<string> results) {
      LaunchManual(error, authenticity, move(results));
    });
  }
  return true;
}

void MoshClientInstance::LaunchManual(Resolver::Error error,
                                      Resolver::Authenticity authenticity,
                                      vector<string> results) {
  if (resolver_->IsValidating()) {
    switch (authenticity) {
      case Resolver::Authenticity::AUTHENTIC:
        Output(TYPE_DISPLAY, "Authenticated DNS lookup.\r\n");
        break;
      case Resolver::Authenticity::INSECURE:
        Output(TYPE_DISPLAY, "Could NOT authenticate DNS lookup.\r\n");
        break;
    }
  }
  if (error == Resolver::Error::NOT_RESOLVED) {
    Error(
        "Could not resolve the hostname. "
        "Check the spelling and the address family.");
    Output(TYPE_EXIT, "");
    return;
  }
  if (error != Resolver::Error::OK) {
    Error("Name resolution failed with unexpected error code: %d", error);
    Output(TYPE_EXIT, "");
    return;
  }
  if (results.size() == 0) {
    Error("There were no addresses.");
    Output(TYPE_EXIT, "");
    return;
  }
  // Only using first address.
  const string& address = results[0];
  int address_len = address.size() + 1;
  addr_ = make_unique<char[]>(address_len);
  strncpy(addr_.get(), address.c_str(), address_len);
  LaunchMosh(0);
}

void MoshClientInstance::LaunchMosh(__attribute__((unused)) int32_t unused) {
  int thread_err = pthread_create(&thread_, nullptr, MoshThread, this);
  if (thread_err != 0) {
    Error("Failed to create Mosh thread: %s", strerror(thread_err));
  }
}

void* MoshClientInstance::MoshThread(void* data) {
  MoshClientInstance* const thiz = reinterpret_cast<MoshClientInstance*>(data);

  setenv("TERM", "xterm-256color", 1);
  if (getenv("LANG") == nullptr) {
    // Chrome cleans the environment, but on Linux and Chrome OS, it leaves
    // $LANG. Mac and Windows don't get this variable, at least not as of
    // 33.0.1750.117. This is critical for ncurses wide character support. We
    // work around this omission here.
    setenv("LANG", "C.UTF-8", 1);
  }

  // Some hoops to avoid a compiler warning.
  const char binary_name[] = "mosh-client";
  auto argv0 = make_unique<char[]>(sizeof(binary_name));
  memcpy(argv0.get(), binary_name, sizeof(binary_name));

  char* argv[] = {argv0.get(), thiz->addr_.get(), thiz->port_.get()};
  thiz->Log("Mosh(): Calling mosh_main");
  mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
  thiz->Log("Mosh(): mosh_main returned");

  thiz->Output(TYPE_EXIT, "");
  return nullptr;
}

void MoshClientInstance::LaunchSSHLogin() {
  ssh_login_.set_host(host_);
  ssh_login_.set_type(type_);
  ssh_login_.set_port(string(port_.get()));
  ssh_login_.set_resolver(resolver_.get());
  setenv("SSH_AUTH_SOCK", "agent", 1);  // Connects to UnixSocketStreamImpl.

  int thread_err = pthread_create(&thread_, nullptr, SSHLoginThread, this);
  if (thread_err != 0) {
    Error("Failed to create SSHLogin thread: %s", strerror(thread_err));
  }
}

void* MoshClientInstance::SSHLoginThread(void* data) {
  MoshClientInstance* const thiz = reinterpret_cast<MoshClientInstance*>(data);

  if (thiz->ssh_login_.Start() == false) {
    thiz->Error("SSH Login failed.");
    thiz->Output(TYPE_EXIT, "");
    return nullptr;
  }

  // Extract Mosh params.
  thiz->port_ = make_unique<char[]>(6);
  memset(thiz->port_.get(), 0, 6);
  thiz->ssh_login_.mosh_port().copy(thiz->port_.get(), 5);
  size_t addr_len = thiz->ssh_login_.mosh_addr().size();
  thiz->addr_ = make_unique<char[]>(addr_len + 1);
  memset(thiz->addr_.get(), 0, addr_len + 1);
  thiz->ssh_login_.mosh_addr().copy(thiz->addr_.get(), addr_len);
  setenv("MOSH_KEY", thiz->ssh_login_.mosh_key().c_str(), 1);

  // Save any updates to known hosts.
  thiz->Output(TYPE_SET_KNOWN_HOSTS, thiz->ssh_login_.known_hosts());

  pp::Module::Get()->core()->CallOnMainThread(
      0, thiz->cc_factory_.NewCallback(&MoshClientInstance::LaunchMosh));

  return nullptr;
}

// Initialize static data for MoshClientInstance.
int MoshClientInstance::num_instances_ = 0;

ssize_t Terminal::Write(const void* buf, size_t count) {
  string s((const char*)buf, count);
  instance_.Output(MoshClientInstance::TYPE_DISPLAY, s);
  return count;
}

ssize_t ErrorLog::Write(const void* buf, size_t count) {
  string s((const char*)buf, count);
  instance_.Output(MoshClientInstance::TYPE_ERROR, s);
  return count;
}

class MoshClientModule : public pp::Module {
 public:
  MoshClientModule() {}
  ~MoshClientModule() override {}

  pp::Instance* CreateInstance(PP_Instance instance) override {
    return new MoshClientInstance(instance);
  }
};

namespace pp {

Module* CreateModule() { return new MoshClientModule(); }

}  // namespace pp

//
// Window size wrapper functions that are too specialized to be
// moved to a general wrapper module.
//

extern "C" {

int sigaction(int signum, const struct sigaction* act,
              struct sigaction* oldact) {
  Log("sigaction(%d, ...)", signum);
  assert(oldact == nullptr);
  switch (signum) {
    case SIGWINCH:
      instance->window_change_->SetHandler(act->sa_handler);
      break;
  }

  return 0;
}

int ioctl(int d, long unsigned int request, ...) {
  if (d != STDIN_FILENO || request != TIOCGWINSZ) {
    Log("ioctl(%d, %u, ...): Got unexpected call", d, request);
    errno = EPROTO;
    return -1;
  }
  va_list argp;
  va_start(argp, request);
  struct winsize* ws = va_arg(argp, struct winsize*);
  ws->ws_row = instance->window_change_->height();
  ws->ws_col = instance->window_change_->width();
  va_end(argp);
  return 0;
}

}  // extern "C"

//
// Functions for pepper_wrapper.h.
//

PepperPOSIX::POSIX& GetPOSIX() { return *instance->posix_; }

void Log(const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  if (instance != nullptr) {
    instance->Logv(MoshClientInstance::TYPE_LOG, format, argp);
  }
  va_end(argp);
}
