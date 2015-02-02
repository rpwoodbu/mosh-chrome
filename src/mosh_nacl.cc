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

#include "mosh_nacl.h"
#include "pthread_locks.h"

#include <algorithm>
#include <deque>
#include <vector>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "irt.h"
#include "ppapi/cpp/module.h"

using ::std::vector;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char *argv[]);

// Used by pepper_wrapper.h functions.
static class MoshClientInstance *instance = NULL;

// Implements most of the plumbing to get keystrokes to Mosh. A tiny amount of
// plumbing is in the MoshClientInstance::HandleMessage().
class Keyboard : public PepperPOSIX::Reader {
 public:
  virtual ssize_t Read(void *buf, size_t count) {
    int num_read = 0;

    pthread::MutexLock m(&keypresses_lock_);

    while (keypresses_.size() > 0 && num_read < count) {
      ((char *)buf)[num_read] = keypresses_.front();
      keypresses_.pop_front();
      ++num_read;
    }

    target_->UpdateRead(keypresses_.size() > 0);

    return num_read;
  }

  // Handle input from the keyboard.
  void HandleInput(string input) {
    if (input.size() == 0) {
      // Nothing to see here.
      return;
    }
    {
      pthread::MutexLock m(&keypresses_lock_);
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
  Terminal(MoshClientInstance *instance) : instance_(instance) {}

  // This has to be defined below MoshClientInstance due to dependence on it.
  virtual ssize_t Write(const void *buf, size_t count);

 private:
  MoshClientInstance *instance_;
};

// Implements the plumbing to get stderr to Javascript.
class ErrorLog : public PepperPOSIX::Writer {
 public:
  ErrorLog(MoshClientInstance *instance) : instance_(instance) {}

  // This has to be defined below MoshClientInstance due to dependence on it.
  virtual ssize_t Write(const void *buf, size_t count);

 private:
  MoshClientInstance *instance_;
};

// Implements the plumbing to get SIGWINCH to Mosh. A tiny amount of plumbing
// is in MoshClientInstance::HandleMessage();
class WindowChange : public PepperPOSIX::Signal {
 public:
  WindowChange() : sigwinch_handler_(NULL), width_(80), height_(24) {}

  // Update geometry and send SIGWINCH.
  void Update(int width, int height) {
    if (sigwinch_handler_ != NULL) {
      width_ = width;
      height_ = height;
      target_->UpdateRead(true);
    }
  }

  void SetHandler(void (*sigwinch_handler)(int)) {
    sigwinch_handler_ = sigwinch_handler;
  }

  virtual void Handle() {
    if (sigwinch_handler_ != NULL) {
      sigwinch_handler_(SIGWINCH);
      target_->UpdateRead(false);
    }
  }

  int height() { return height_; }
  int width() { return width_; }

 private:
  int width_;
  int height_;
  void (*sigwinch_handler_)(int);
};

class DevURandom : public PepperPOSIX::Reader {
 public:
  DevURandom() {
    nacl_interface_query(NACL_IRT_RANDOM_v0_1, &random_, sizeof(random_));
  }

  virtual ssize_t Read(void *buf, size_t count) {
    size_t bytes_read = 0;
    random_.get_random_bytes(buf, count, &bytes_read);
    return bytes_read;
  }

 private:
  struct nacl_irt_random random_;
};

// DevURandom factory for registration with PepperPOSIX::POSIX.
PepperPOSIX::File *DevURandomFactory() {
  return new DevURandom();
}

MoshClientInstance::MoshClientInstance(PP_Instance instance) :
    pp::Instance(instance), addr_(NULL), port_(NULL), ssh_mode_(false),
    posix_(NULL), keyboard_(NULL), instance_handle_(this),
    window_change_(NULL), resolver_(instance_handle_), cc_factory_(this) {
  ++num_instances_;
  assert (num_instances_ == 1);
  ::instance = this;
}

MoshClientInstance::~MoshClientInstance() {
  // Wait for thread to finish.
  if (thread_) {
    pthread_join(thread_, NULL);
  }
  delete[] addr_;
  delete[] port_;
  delete posix_;
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

void MoshClientInstance::Logv(OutputType t, const char *format, va_list argp) {
  char buf[1024];
  int size = vsnprintf(buf, sizeof(buf), format, argp);
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
  bool got_addr = false;
  const char *secret;
  for (int i = 0; i < argc; ++i) {
    string name = argn[i];
    int len = strlen(argv[i]) + 1;
    if (name == "key") {
      secret = argv[i];
    } else if (name == "addr" && addr_ == NULL) {
      // TODO: Support IPv6 when Mosh does.
      const PP_HostResolver_Hint hint = {PP_NETADDRESS_FAMILY_IPV4, 0};
      // Mosh will launch via this callback when the resolution completes.
      resolver_.Resolve(argv[i], 0, hint,
          cc_factory_.NewCallback(&MoshClientInstance::Launch));
      got_addr = true;
    } else if (name == "port" && port_ == NULL) {
      port_ = new char[len];
      strncpy(port_, argv[i], len);
    } else if (name == "mode") {
      if (string(argv[i]) == "ssh") {
        ssh_mode_ = true;
      }
    } else if (name == "user") {
      ssh_login_.set_user(argv[i]);
    } else if (name == "command") {
      ssh_login_.set_command(argv[i]);
    }
  }

  if (got_addr == false || port_ == NULL) {
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

  // Setup communications.
  keyboard_ = new Keyboard();
  Terminal *terminal = new Terminal(this);
  ErrorLog *error_log = new ErrorLog(this);
  window_change_ = new WindowChange();
  posix_ = new PepperPOSIX::POSIX(
      instance_handle_, keyboard_, terminal, error_log, window_change_);
  posix_->RegisterFile("/dev/urandom", DevURandomFactory);

  // Mosh will launch via the resolution callback (see above).
  return true;
}

void MoshClientInstance::Launch(int32_t result) {
  if (result != PP_OK) {
    Error("Resolution failed: %d", result);
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
  addr_ = new char[addr_len];
  strncpy(addr_, addr_str.c_str(), addr_len);

  if (ssh_mode_) {
    // HandleMessage() will call LaunchSSHLogin().
    Output(TYPE_GET_SSH_KEY, "");
  } else {
    LaunchMosh(0);
  }
}

void MoshClientInstance::LaunchMosh(int32_t unused) {
  int thread_err = pthread_create(&thread_, NULL, MoshThread, this);
  if (thread_err != 0) {
    Error("Failed to create Mosh thread: %s", strerror(thread_err));
  }
}

void *MoshClientInstance::MoshThread(void *data) {
  MoshClientInstance *thiz = reinterpret_cast<MoshClientInstance *>(data);

  setenv("TERM", "xterm-256color", 1);
  if (getenv("LANG") == NULL) {
    // Chrome cleans the environment, but on Linux and Chrome OS, it leaves
    // $LANG. Mac and Windows don't get this variable, at least not as of
    // 33.0.1750.117. This is critical for ncurses wide character support. We
    // work around this omission here.
    setenv("LANG", "C.UTF-8", 1);
  }

  // Some hoops to avoid a compiler warning.
  const char *binary_name = "mosh-client";
  char *argv0 = new char[sizeof(*binary_name)];
  memcpy(argv0, binary_name, sizeof(*argv0));

  char *argv[] = { argv0, thiz->addr_, thiz->port_ };
  thiz->Log("Mosh(): Calling mosh_main");
  mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
  thiz->Log("Mosh(): mosh_main returned");

  delete[] argv0;
  thiz->Output(TYPE_EXIT, "");
  return NULL;
}

void MoshClientInstance::LaunchSSHLogin() {
  ssh_login_.set_addr(addr_);
  ssh_login_.set_port(port_);

  int thread_err = pthread_create(&thread_, NULL, SSHLoginThread, this);
  if (thread_err != 0) {
    Error("Failed to create SSHLogin thread: %s", strerror(thread_err));
  }
}

void *MoshClientInstance::SSHLoginThread(void *data) {
  MoshClientInstance *thiz = reinterpret_cast<MoshClientInstance *>(data);

  if (thiz->ssh_login_.Start() == false) {
    thiz->Error("SSH Login failed.");
    thiz->Output(TYPE_EXIT, "");
    return NULL;
  }

  // Extract Mosh params.
  delete[] thiz->port_;
  thiz->port_ = new char[6];
  memset(thiz->port_, 0, 6);
  thiz->ssh_login_.mosh_port().copy(thiz->port_, 5);
  delete[] thiz->addr_;
  size_t addr_len = thiz->ssh_login_.mosh_addr().size();
  thiz->addr_ = new char[addr_len+1];
  memset(thiz->addr_, 0, addr_len+1);
  thiz->ssh_login_.mosh_addr().copy(thiz->addr_, addr_len);
  setenv("MOSH_KEY", thiz->ssh_login_.mosh_key().c_str(), 1);

  // Save any updates to known hosts.
  thiz->Output(TYPE_SET_KNOWN_HOSTS, thiz->ssh_login_.known_hosts());

  pp::Module::Get()->core()->CallOnMainThread(
      0, thiz->cc_factory_.NewCallback(&MoshClientInstance::LaunchMosh));

  return NULL;
}

// Initialize static data for MoshClientInstance.
int MoshClientInstance::num_instances_ = 0;

ssize_t Terminal::Write(const void *buf, size_t count) {
  string s((const char *)buf, count);
  instance_->Output(MoshClientInstance::TYPE_DISPLAY, s);
  return count;
}

ssize_t ErrorLog::Write(const void *buf, size_t count) {
  string s((const char *)buf, count);
  instance_->Output(MoshClientInstance::TYPE_ERROR, s);
  return count;
}

class MoshClientModule : public pp::Module {
 public:
  MoshClientModule() : pp::Module() {}
  virtual ~MoshClientModule() {}

  virtual pp::Instance *CreateInstance(PP_Instance instance) {
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
  assert(oldact == NULL);
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

PepperPOSIX::POSIX *GetPOSIX() {
  return instance->posix_;
}

void Log(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  if (instance != NULL) {
    instance->Logv(MoshClientInstance::TYPE_LOG, format, argp);
  }
  va_end(argp);
}
