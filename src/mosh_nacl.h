// mosh_nacl.h - Mosh for Native Client (NaCl).
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

#ifndef MOSH_NACL_H
#define MOSH_NACL_H

#include "pepper_wrapper.h"
#include "ssh_login.h"

#include <memory>
#include <string>
#include <pthread.h>

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

using std::string;

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance);
  ~MoshClientInstance() override;

  // Instance initialization method. Called by the Pepper API.
  bool Init(uint32_t argc, const char *argn[], const char *argv[]) override;

  // Handles messages from Javascript. Called by the Pepper API.
  void HandleMessage(const pp::Var &var) override;

  // Type of data to output to Javascript.
  enum OutputType {
    TYPE_DISPLAY = 0,
    TYPE_LOG,
    TYPE_ERROR,
    TYPE_GET_SSH_KEY,
    TYPE_GET_KNOWN_HOSTS,
    TYPE_SET_KNOWN_HOSTS,
    TYPE_EXIT,
  };

  // Low-level function to output data to Javascript.
  void Output(OutputType t, const pp::Var &data);

  // Sends messages to the Javascript console log.
  void Logv(OutputType t, const std::string& format, va_list argp);

  // Sends messages to the Javascript console log.
  void Log(const char *format, ...);

  // Sends error messages to the Javascript console log and terminal.
  void Error(const char *format, ...);

  // Pepper POSIX emulation.
  std::unique_ptr<PepperPOSIX::POSIX> posix_;

  // Window change "file"; must be visible to sigaction().
  // Class POSIX takes ownership of this, but keeping pointer for convenience.
  class WindowChange* window_change_ = nullptr;

 private:
  // Launcher that can be a callback.
  void Launch(int32_t result);

  // Launches Mosh in a new thread. Must take one argument to be used as a
  // completion callback.
  void LaunchMosh(int32_t unused);

  // New thread entry point for Mosh. |data| is |this|.
  static void *MoshThread(void *data);

  // Launches SSHLogin in a new thread.
  void LaunchSSHLogin();

  // New thread entry point for SSHLogin. |data| is |this|.
  static void *SSHLoginThread(void *data);

  static int num_instances_; // This needs to be a singleton.
  pthread_t thread_ = nullptr;

  // Non-const params for mosh_main().
  std::unique_ptr<char[]> addr_;
  std::unique_ptr<char[]> port_;

  bool ssh_mode_ = false;
  SSHLogin ssh_login_;

  pp::InstanceHandle instance_handle_ = this;
  // Class POSIX takes ownership of this, but keeping pointer for convenience.
  class Keyboard* keyboard_ = nullptr;
  pp::HostResolver resolver_;
  pp::CompletionCallbackFactory<MoshClientInstance> cc_factory_;

  // Disable copy and assignment.
  MoshClientInstance(const MoshClientInstance&) = delete;
  MoshClientInstance &operator=(const MoshClientInstance&) = delete;
};

#endif // MOSH_NACL_H
