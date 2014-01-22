// mosh_nacl.cc - Mosh for Native Client (NaCl).
//
// This file contains the Mosh-specific bits of the NaCl port of the
// client.

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

#include "pepper_wrapper.h"
#include "ssh.h"

#include <algorithm>
#include <deque>
#include <string>
#include <vector>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "irt.h"
#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

using ::std::string;
using ::std::vector;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char *argv[]);

// Used by pepper_wrapper.h functions.
static class MoshClientInstance *instance = NULL;

// Implements most of the plumbing to get keystrokes to Mosh. A tiny amount of
// plumbing is in the MoshClientInstance::HandleMessage().
class Keyboard : public PepperPOSIX::Reader {
 public:
  Keyboard() {
    pthread_mutex_init(&keypresses_lock_, NULL);
  }
  virtual ~Keyboard() {
    pthread_mutex_destroy(&keypresses_lock_);
  }

  virtual ssize_t Read(void *buf, size_t count) {
    // TODO: Could submit in batches, but rarely will get in batches.
    int result = 0;
    pthread_mutex_lock(&keypresses_lock_);
    if (keypresses_.size() > 0) {
      ((char *)buf)[0] = keypresses_.front();
      keypresses_.pop_front();
      target_->UpdateRead(keypresses_.size() > 0);
      result = 1;
    } else {
      // Cannot use Log() here; circular dependency.
      fprintf(stderr,
          "Keyboard::Read(): From STDIN, no data, treat as nonblocking.\n");
    }
    pthread_mutex_unlock(&keypresses_lock_);
    return result;
  }

  // Handle input from the keyboard.
  void HandleInput(string input) {
    pthread_mutex_lock(&keypresses_lock_);
    for (int i = 0; i < input.size(); ++i) {
      keypresses_.push_back(input[i]);
    }
    pthread_mutex_unlock(&keypresses_lock_);
    target_->UpdateRead(true);
  }

 private:
  // Queue of keyboard keypresses.
  std::deque<char> keypresses_; // Guard with keypresses_lock_.
  pthread_mutex_t keypresses_lock_;
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

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) :
      pp::Instance(instance), addr_(NULL), port_(NULL), ssh_mode_(false),
      posix_(NULL), keyboard_(NULL), instance_handle_(this),
      window_change_(NULL), resolver_(instance_handle_), cc_factory_(this) {
    ++num_instances_;
    assert (num_instances_ == 1);
    ::instance = this;
  }

  virtual ~MoshClientInstance() {
    // Wait for thread to finish.
    if (thread_) {
      pthread_join(thread_, NULL);
    }
    delete[] addr_;
    delete[] port_;
    delete posix_;
  }

  virtual void HandleMessage(const pp::Var &var) {
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
        ssh_key_ = key.AsString();
      }
      LaunchSSHLogin();
    } else {
      Log("HandleMessage(): Got a message of an unexpected type.");
    }
  }

  // Type of data to output to Javascript.
  enum OutputType {
    TYPE_DISPLAY = 0,
    TYPE_LOG,
    TYPE_ERROR,
    TYPE_GET_SSH_KEY,
  };

  // Low-level function to output data to Javascript.
  void Output(OutputType t, const string &s) {
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
     default:
      // Bad type.
      return;
    }

    pp::VarDictionary dict;
    dict.Set(pp::Var("type"), pp::Var(type));
    dict.Set(pp::Var("data"), pp::Var(s));
    PostMessage(pp::Var(dict));
  }

  // Sends messages to the Javascript console log.
  void Logv(OutputType t, const char *format, va_list argp) {
    char buf[1024];
    int size = vsnprintf(buf, sizeof(buf), format, argp);
    Output(t, string((const char *)buf, size));
  }

  // Sends messages to the Javascript console log.
  void Log(const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    Logv(TYPE_LOG, format, argp);
    va_end(argp);
  }

  // Sends error messages to the Javascript console log and terminal.
  void Error(const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    Logv(TYPE_ERROR, format, argp);
    va_end(argp);
  }

  virtual bool Init(uint32_t argc, const char *argn[], const char *argv[]) {
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
        ssh_user_ = argv[i];
      }
    }

    if (got_addr == false || port_ == NULL) {
      Log("Must supply addr and port attributes.");
      return false;
    }

    if (ssh_mode_) {
      if (ssh_user_.size() == 0) {
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

  void Launch(int32_t result) {
    if (result != PP_OK) {
      Error("Resolution failed: %d", result);
      return;
    }
    if (resolver_.GetNetAddressCount() < 1) {
      Error("There were no addresses.");
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

  void LaunchSSHLogin() {
    int thread_err = pthread_create(&thread_, NULL, SSHLogin, this);
    if (thread_err != 0) {
      Error("Failed to create SSH login thread: %s", strerror(thread_err));
    }
  }

  // Mosh launcher that can be a callback.
  void LaunchMosh(int32_t result) {
    int thread_err = pthread_create(&thread_, NULL, Mosh, this);
    if (thread_err != 0) {
      Error("Failed to create Mosh thread: %s", strerror(thread_err));
    }
  }

  static void *Mosh(void *data) {
    MoshClientInstance *thiz = reinterpret_cast<MoshClientInstance *>(data);

    setenv("TERM", "xterm-256color", 1);
    char *argv[] = { "mosh-client", thiz->addr_, thiz->port_ };
    thiz->Log("Mosh(): Calling mosh_main");
    mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
    thiz->Log("Mosh(): mosh_main returned");
    return 0;
  }

  // Used by SSHLogin() to get a line of input from the keyboard.
  static void GetKeyboardLine(char *buf, size_t len) {
    int i = 0;
    while (i < len) {
      char in = getchar();
      // Handle return/enter.
      if (in == '\r') {
        break;
      }
      // Handle backspace.
      if (in == 0x8 || in == 0x7f) {
        if (i > 0) {
          --i;
        }
        continue;
      }
      buf[i] = in;
      ++i;
    }
    buf[i] = 0;
  }

  // Get MOSH_KEY via SSH.
  static void *SSHLogin(void *data) {
    MoshClientInstance *thiz = reinterpret_cast<MoshClientInstance *>(data);

    setenv("HOME", "dummy", 1); // To satisfy libssh.
    ssh::Session s(thiz->addr_, atoi(thiz->port_), thiz->ssh_user_);
    if (s.Connect() == false) {
      thiz->Error("Could not connect via ssh: %s", s.GetLastError().c_str());
      return NULL;
    }
    // TODO: This _really_ needs to be more secure.
    thiz->Output(TYPE_DISPLAY,
        "Fingerprint of remote ssh host (MD5): " +
        s.GetPublicKey()->MD5() + "\r\n");

    // Place the list of supported authentications types here, in the order
    // they should be tried.
    vector<ssh::AuthenticationType> client_auths;
    client_auths.push_back(ssh::kPublicKey);
    client_auths.push_back(ssh::kInteractive);
    client_auths.push_back(ssh::kPassword);

    thiz->Output(TYPE_DISPLAY, "Authentication types supported by server:\r\n");
    vector<ssh::AuthenticationType> server_auths = s.GetAuthenticationTypes();
    if (server_auths.size() == 0) {
      thiz->Error("Failed to get authentication types: %s",
          s.GetLastError().c_str());
      return NULL;
    }
    for (vector<ssh::AuthenticationType>::iterator i = server_auths.begin();
      i != server_auths.end();
      ++i) {
      thiz->Output(TYPE_DISPLAY, " - " + ssh::GetAuthenticationTypeName(*i));
      if (std::find(client_auths.begin(), client_auths.end(), *i) ==
          client_auths.end()) {
        thiz->Output(TYPE_DISPLAY, " (not supported by client)");
      }
      thiz->Output(TYPE_DISPLAY, "\r\n");
    }

    bool authenticated = false;
    for (vector<ssh::AuthenticationType>::iterator i = client_auths.begin();
        authenticated == false && i != client_auths.end();
        ++i) {
      if (std::find(server_auths.begin(), server_auths.end(), *i) ==
          server_auths.end()) {
        // Not supported by server, moving on.
        continue;
      }

      thiz->Output(TYPE_DISPLAY,
        "Trying authentication type " + ssh::GetAuthenticationTypeName(*i) +
        "\r\n");

      ssh::KeyboardInteractive *kbd = NULL;
      ssh::KeyboardInteractive::Status status =
          ssh::KeyboardInteractive::kPending;
      char input[256];

      switch(*i) {
        case ssh::kPassword:
          thiz->Output(TYPE_DISPLAY, "Password: ");
          GetKeyboardLine(input, sizeof(input));
          thiz->Output(TYPE_DISPLAY, "\r\n");
          authenticated = s.AuthUsingPassword(input);
          // For safety, zero the sensitive input ASAP.
          memset(input, 0, sizeof(input));
          if (authenticated == false) {
            thiz->Error("Password authentication failed: %s",
                s.GetLastError().c_str());
          }
          break;

        case ssh::kInteractive:
          kbd = s.AuthUsingKeyboardInteractive();
          status = kbd->GetStatus();
          while (status == ssh::KeyboardInteractive::kPending) {
            thiz->Output(TYPE_DISPLAY, kbd->GetName());
            thiz->Output(TYPE_DISPLAY, kbd->GetInstruction());
            bool done = false;
            while (!done) {
              thiz->Output(TYPE_DISPLAY, kbd->GetNextPrompt());
              GetKeyboardLine(input, sizeof(input));
              thiz->Output(TYPE_DISPLAY, "\r\n");
              done = kbd->Answer(input);
              // For safety, zero the sensitive input ASAP.
              memset(input, 0, sizeof(input));
            }
            status = kbd->GetStatus();
          }
          if (status != ssh::KeyboardInteractive::kAuthenticated) {
            thiz->Error("Keyboard interactive auth failed or insufficient.");
            break;
          }
          authenticated = true;
          break;

        case ssh::kPublicKey:
          if (thiz->ssh_key_.size() == 0) {
            thiz->Output(TYPE_DISPLAY, "No ssh key found.\r\n");
          } else {
            thiz->Output(TYPE_DISPLAY, "Passphrase: ");
            GetKeyboardLine(input, sizeof(input));
            thiz->Output(TYPE_DISPLAY, "\r\n");
            ssh::Key key;
            bool result = key.ImportPrivateKey(thiz->ssh_key_, input);
            // For safety, zero the sensitive input ASAP.
            memset(input, 0, sizeof(input));
            thiz->ssh_key_.clear();
            if (result == false) {
              thiz->Error("Error reading key: %s", s.GetLastError().c_str());
              break;
            }
            if (s.AuthUsingKey(key) == false) {
              thiz->Error("Key auth failed: %s", s.GetLastError().c_str());
              break;
            }
            authenticated = true;
          }
          break;

        default:
          // Should not get here.
          assert(false);
      }

      // For safety, and paranoia, zero the sensitive input here to be sure it
      // is always done.
      memset(input, 0, sizeof(input));
      thiz->ssh_key_.clear();
    }

    if (authenticated == false) {
      thiz->Error("ssh authentication failed: %s", s.GetLastError().c_str());
      return NULL;
    }

    ssh::Channel *c = s.NewChannel();
    if (c->Execute("mosh-server new -s -c 256 -l LANG=en_US.UTF-8") == false) {
      thiz->Error("Failed to execute mosh-server: %s",
          s.GetLastError().c_str());
      return NULL;
    }
    string buf;
    if (c->Read(&buf, NULL) == false) {
      thiz->Error("Error reading from remote ssh server: %s",
          s.GetLastError().c_str());
      return NULL;
    }

    char key[23];
    thiz->port_ = new char[6];
    int result = sscanf(buf.c_str(), "\r\nMOSH CONNECT %5s %22s\r\n",
        thiz->port_, key);
    if (result != 2) {
      thiz->Error("Bad response when running mosh-server: '%s'", buf.c_str());
      return NULL;
    }

    setenv("MOSH_KEY", key, 1);
    pp::Module::Get()->core()->CallOnMainThread(
        0, thiz->cc_factory_.NewCallback(&MoshClientInstance::LaunchMosh));
    return NULL;
  }

  // Pepper POSIX emulation.
  PepperPOSIX::POSIX *posix_;
  // Window change "file"; must be visible to sigaction().
  WindowChange *window_change_;

 private:
  static int num_instances_; // This needs to be a singleton.
  pthread_t thread_;

  // Non-const params for mosh_main().
  char *addr_;
  char *port_;

  bool ssh_mode_;
  string ssh_user_;
  string ssh_key_;

  pp::InstanceHandle instance_handle_;
  Keyboard *keyboard_;
  pp::CompletionCallbackFactory<MoshClientInstance> cc_factory_;
  pp::HostResolver resolver_;
};

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

//
// Function for SSH logging.
//

extern "C" {
void __wrap__ssh_log(int verbosity, const char *function,
    const char *format, ...) {
  if (verbosity > SSH_LOG_WARNING) {
    return;
  }
  string f = string("libssh: ") + function + "(): " + format;
  va_list argp;
  va_start(argp, format);
  if (instance != NULL) {
    instance->Logv(MoshClientInstance::TYPE_LOG, f.c_str(), argp);
  }
  va_end(argp);
}
} // extern "C"
