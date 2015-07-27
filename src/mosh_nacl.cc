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

#include "make_unique.h"
#include "mosh_nacl.h"
#include "pthread_locks.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <vector>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "irt.h"
#include "ppapi/cpp/module.h"

using std::function;
using std::move;
using std::unique_ptr;
using std::vector;
using util::make_unique;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char *argv[]);

// Used by pepper_wrapper.h functions.
static class MoshClientInstance *instance = nullptr;

// Implements most of the plumbing to get keystrokes to Mosh. A tiny amount of
// plumbing is in the MoshClientInstance::HandleMessage().
class Keyboard : public PepperPOSIX::Reader {
 public:
  ssize_t Read(void *buf, size_t count) override {
    int num_read = 0;

    pthread::MutexLock m(keypresses_lock_);

    while (keypresses_.size() > 0 && num_read < count) {
      ((char *)buf)[num_read] = keypresses_.front();
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
  std::deque<char> keypresses_; // Guard with keypresses_lock_.
  pthread::Mutex keypresses_lock_;
};

// Implements the plumbing to get stdout to the terminal.
class Terminal : public PepperPOSIX::Writer {
 public:
  Terminal(MoshClientInstance& instance) : instance_(instance) {}

  // This has to be defined below MoshClientInstance due to dependence on it.
  ssize_t Write(const void *buf, size_t count) override;

 private:
  MoshClientInstance& instance_;
};

// Implements the plumbing to get stderr to Javascript.
class ErrorLog : public PepperPOSIX::Writer {
 public:
  ErrorLog(MoshClientInstance& instance) : instance_(instance) {}

  // This has to be defined below MoshClientInstance due to dependence on it.
  ssize_t Write(const void *buf, size_t count) override;

 private:
  MoshClientInstance& instance_;
};

// Implements the plumbing to get SIGWINCH to Mosh. A tiny amount of plumbing
// is in MoshClientInstance::HandleMessage();
class WindowChange : public PepperPOSIX::Signal {
 public:
  // Update geometry and send SIGWINCH.
  void Update(int width, int height) {
    if (sigwinch_handler_ != nullptr) {
      width_ = width;
      height_ = height;
      target_->UpdateRead(true);
    }
  }

  void SetHandler(function<void (int)> sigwinch_handler) {
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
  function<void (int)> sigwinch_handler_;
};

class DevURandom : public PepperPOSIX::Reader {
 public:
  DevURandom() {
    nacl_interface_query(NACL_IRT_RANDOM_v0_1, &random_, sizeof(random_));
  }

  ssize_t Read(void *buf, size_t count) override {
    size_t bytes_read = 0;
    random_.get_random_bytes(buf, count, &bytes_read);
    return bytes_read;
  }

 private:
  struct nacl_irt_random random_;
};

// DevURandom factory for registration with PepperPOSIX::POSIX.
unique_ptr<PepperPOSIX::File> DevURandomFactory() {
  return make_unique<DevURandom>();
}

MoshClientInstance::MoshClientInstance(PP_Instance instance) :
    pp::Instance(instance), resolver_(instance_handle_), cc_factory_(this) {
  ++num_instances_;
  assert (num_instances_ == 1);
  ::instance = this;
}

MoshClientInstance::~MoshClientInstance() {
  // Wait for thread to finish.
  if (thread_) {
    pthread_join(thread_, nullptr);
  }
}

void MoshClientInstance::HandleMessage(const pp::Var &var) {
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
  } else {
    Log("HandleMessage(): Got a message of an unexpected type.");
  }
}

void MoshClientInstance::Output(OutputType t, const pp::Var &data) {
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

void MoshClientInstance::Logv(
    OutputType t, const string& format, va_list argp) {
  char buf[1024];
  int size = vsnprintf(buf, sizeof(buf), format.c_str(), argp);
  Output(t, string((const char *)buf, size));
}

void MoshClientInstance::Log(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  Logv(TYPE_LOG, format, argp);
  va_end(argp);
}

void MoshClientInstance::Error(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  Logv(TYPE_ERROR, format, argp);
  va_end(argp);
}

bool MoshClientInstance::Init(
    uint32_t argc, const char *argn[], const char *argv[]) {
  const char *secret = nullptr;
  string addr;
  string family;
  for (int i = 0; i < argc; ++i) {
    string name = argn[i];
    int len = strlen(argv[i]) + 1;
    if (name == "key") {
      secret = argv[i];
    } else if (name == "addr" && addr_ == nullptr) {
      addr = argv[i];
    } else if (name == "port" && port_ == nullptr) {
      port_ = make_unique<char[]>(len);
      strncpy(port_.get(), argv[i], len);
    } else if (name == "family") {
      family = argv[i];
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
    }
  }

  if (addr.size() == 0 || port_ == nullptr) {
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

  PP_HostResolver_Hint hint;
  if (family == "IPv4") {
    hint = {PP_NETADDRESS_FAMILY_IPV4, 0};
  } else if (family == "IPv6") {
    hint = {PP_NETADDRESS_FAMILY_IPV6, 0};
  } else {
    Log("Must supply family attribute (IPv4 or IPv6).");
    return false;
  }
  // Mosh will launch via this callback when the resolution completes.
  resolver_.Resolve(addr.c_str(), 0, hint,
    cc_factory_.NewCallback(&MoshClientInstance::Launch));

  // Setup communications. We keep pointers to |keyboard_| and
  // |window_change_|, as we need to access their specialized methods. |posix_|
  // owns them, but we own |posix_|, so it is all good so long as these "files"
  // are not closed.
  auto keyboard = make_unique<Keyboard>();
  auto window_change = make_unique<WindowChange>();
  keyboard_ = keyboard.get();
  window_change_ = window_change.get();
  posix_ = make_unique<PepperPOSIX::POSIX>(
     instance_handle_,
     move(keyboard),
     make_unique<Terminal>(*this),
     make_unique<ErrorLog>(*this),
     move(window_change));
  posix_->RegisterFile("/dev/urandom", DevURandomFactory);

  // Mosh will launch via the resolution callback (see above).
  return true;
}

void MoshClientInstance::Launch(int32_t result) {
  if (result == PP_ERROR_NAME_NOT_RESOLVED) {
    Error("Could not resolve the hostname. "
        "Check the spelling and the address family.");
    Output(TYPE_EXIT, "");
    return;
  }
  if (result != PP_OK) {
    Error("Name resolution failed with unexpected error code: %d", result);
    Output(TYPE_EXIT, "");
    return;
  }
  if (resolver_.GetNetAddressCount() < 1) {
    Error("There were no addresses.");
    Output(TYPE_EXIT, "");
    return;
  }
  pp::NetAddress address = resolver_.GetNetAddress(0);
  string addr_str = address.DescribeAsString(false).AsString();
  int addr_len = addr_str.size() + 1;
  addr_ = make_unique<char[]>(addr_len);
  strncpy(addr_.get(), addr_str.c_str(), addr_len);

  if (ssh_mode_) {
    // HandleMessage() will call LaunchSSHLogin().
    Output(TYPE_GET_SSH_KEY, "");
  } else {
    LaunchMosh(0);
  }
}

void MoshClientInstance::LaunchMosh(int32_t unused) {
  int thread_err = pthread_create(&thread_, nullptr, MoshThread, this);
  if (thread_err != 0) {
    Error("Failed to create Mosh thread: %s", strerror(thread_err));
  }
}

void *MoshClientInstance::MoshThread(void *data) {
  MoshClientInstance* const thiz = reinterpret_cast<MoshClientInstance *>(data);

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

  char *argv[] = { argv0.get(), thiz->addr_.get(), thiz->port_.get() };
  thiz->Log("Mosh(): Calling mosh_main");
  mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
  thiz->Log("Mosh(): mosh_main returned");

  thiz->Output(TYPE_EXIT, "");
  return nullptr;
}

void MoshClientInstance::LaunchSSHLogin() {
  ssh_login_.set_addr(string(addr_.get()));
  ssh_login_.set_port(string(port_.get()));

  int thread_err = pthread_create(&thread_, nullptr, SSHLoginThread, this);
  if (thread_err != 0) {
    Error("Failed to create SSHLogin thread: %s", strerror(thread_err));
  }
}

void *MoshClientInstance::SSHLoginThread(void *data) {
  MoshClientInstance* const thiz = reinterpret_cast<MoshClientInstance *>(data);

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
  thiz->addr_ = make_unique<char[]>(addr_len+1);
  memset(thiz->addr_.get(), 0, addr_len+1);
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

ssize_t Terminal::Write(const void *buf, size_t count) {
  string s((const char *)buf, count);
  instance_.Output(MoshClientInstance::TYPE_DISPLAY, s);
  return count;
}

ssize_t ErrorLog::Write(const void *buf, size_t count) {
  string s((const char *)buf, count);
  instance_.Output(MoshClientInstance::TYPE_ERROR, s);
  return count;
}

class MoshClientModule : public pp::Module {
 public:
  MoshClientModule() {}
  ~MoshClientModule() override {}

  pp::Instance *CreateInstance(PP_Instance instance) override {
    return new MoshClientInstance(instance);
  }
};

namespace pp {

Module *CreateModule() {
  return new MoshClientModule();
}

} // namespace pp

//
// Window size wrapper functions that are too specialized to be
// moved to a general wrapper module.
//

int sigaction(int signum, const struct sigaction *act,
    struct sigaction *oldact) {
  Log("sigaction(%d, ...)", signum);
  assert(oldact == nullptr);
  switch (signum) {
  case SIGWINCH:
    instance->window_change_->SetHandler(act->sa_handler);
    break;
  }

  return 0;
}

#ifdef USE_NEWLIB
int ioctl(int d, int request, ...) {
#else
int ioctl(int d, long unsigned int request, ...) {
#endif
  if (d != STDIN_FILENO || request != TIOCGWINSZ) {
    Log("ioctl(%d, %u, ...): Got unexpected call", d, request);
    errno = EPROTO;
    return -1;
  }
  va_list argp;
  va_start(argp, request);
  struct winsize *ws = va_arg(argp, struct winsize*);
  ws->ws_row = instance->window_change_->height();
  ws->ws_col = instance->window_change_->width();
  va_end(argp);
  return 0;
}

//
// Functions for pepper_wrapper.h.
//

PepperPOSIX::POSIX& GetPOSIX() {
  return *instance->posix_;
}

void Log(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  if (instance != nullptr) {
    instance->Logv(MoshClientInstance::TYPE_LOG, format, argp);
  }
  va_end(argp);
}
