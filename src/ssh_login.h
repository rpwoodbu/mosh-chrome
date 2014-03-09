// ssh_login.h - SSH Login for Mosh.
//
// This file contains the Mosh-specific bits of the NaCl port of the
// client.

// Copyright 2013, 2014 Richard Woodbury
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

#include <pthread.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "ssh.h"

using ::std::string;
using ::std::vector;

class MoshClientInstance;

// SSHLogin takes care of the SSH connection and conversation to initiate the
// Mosh session.
class SSHLogin {
 public:
  explicit SSHLogin(MoshClientInstance *mosh);
  ~SSHLogin();

  // Launch login in new thread.
  void Launch();

  string addr() const { return addr_; }
  void set_addr(const string &addr) { addr_ = addr; }

  string port() const { return port_; }
  void set_port(const string &port) { port_ = port; }

  string user() const { return user_; }
  void set_user(const string &user) { user_ = user; }

  string key() const { return key_; }
  void set_key(const string &key) { key_ = key; }

 private:
  // Entry point after thread is created. Handles communication of the
  // disposition of SSHLogin to MoshClientInstance.
  void Start();

  // The real work of the SSHLogin begins here; called from Start().
  bool RealStart();

  // Passed as function pointer to pthread_create(). |data| is |this|.
  static void *ThreadEntry(void *data);

  // Get a line of input from the keyboard.
  static void GetKeyboardLine(char *buf, size_t len, bool echo);

  // Display and check the remote server fingerprint.
  bool CheckFingerprint();

  // Returns the intersection of authentication types that both the client and
  // the server support. Returns NULL on error. Ownership is transferred to the
  // caller.
  vector<ssh::AuthenticationType> *GetAuthTypes();

  bool DoPasswordAuth();
  bool DoInteractiveAuth();
  bool DoPublicKeyAuth();
  bool DoConversation();

  string addr_;
  string port_;
  string user_;
  string key_;
  MoshClientInstance *mosh_;
  pthread_t thread_;
  ssh::Session *session_;

  // Disable copy and assignment.
  SSHLogin(const SSHLogin &);
  SSHLogin &operator=(const SSHLogin &);
};

#endif // SSH_LOGIN_H
