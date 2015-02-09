// ssh_login.h - SSH Login for Mosh.

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

#ifndef SSH_LOGIN_H
#define SSH_LOGIN_H

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "ssh.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"

// SSHLogin takes care of the SSH connection and conversation to initiate the
// Mosh session.
class SSHLogin {
 public:
  SSHLogin();
  ~SSHLogin();

  // Begin the SSH login session. Returns true iff SSH Login succeeded.
  bool Start();

  std::string addr() const { return addr_; }
  void set_addr(const std::string &addr) { addr_ = addr; }

  std::string port() const { return port_; }
  void set_port(const std::string &port) { port_ = port; }

  std::string user() const { return user_; }
  void set_user(const std::string &user) { user_ = user; }

  std::string key() const { return key_; }
  void set_key(const std::string &key) { key_ = key; }

  // The command to run on the remote host. Set to the empty string to use the
  // default.
  std::string command() const { return command_; }
  void set_command(const std::string &command) { command_ = command; }

  pp::VarDictionary known_hosts() const { return known_hosts_; }
  void set_known_hosts(const pp::Var &var) { known_hosts_ = var; }

  std::string mosh_port() const { return mosh_port_; }

  std::string mosh_key() const { return mosh_key_; }

  std::string mosh_addr() const { return mosh_addr_; }

 private:
  // Passed as function pointer to pthread_create(). |data| is |this|.
  static void *ThreadEntry(void *data);

  // Get a line of input from the keyboard.
  static void GetKeyboardLine(char *buf, size_t len, bool echo);

  // Display and check the remote server fingerprint.
  bool CheckFingerprint();

  // Returns the intersection of authentication types that both the client and
  // the server support. Returns nullptr on error.
  std::unique_ptr<std::vector<ssh::AuthenticationType>> GetAuthTypes();

  // Ask a yes/no question to the user, and return the answer as a bool.
  // Prefers to return false if input is not parseable.
  bool AskYesNo(const std::string &prompt);

  bool DoPasswordAuth();
  bool DoInteractiveAuth();
  bool DoPublicKeyAuth();
  bool DoConversation();

  std::string addr_;
  std::string port_;
  std::string user_;
  std::string key_;
  std::string command_;
  std::string mosh_port_;
  std::string mosh_key_;
  std::string mosh_addr_;
  pp::VarDictionary known_hosts_;
  std::unique_ptr<ssh::Session> session_;

  // Disable copy and assignment.
  SSHLogin(const SSHLogin &);
  SSHLogin &operator=(const SSHLogin &);
};

#endif // SSH_LOGIN_H
