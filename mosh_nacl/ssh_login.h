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

#include "ssh.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "resolver.h"

// SSHLogin takes care of the SSH connection and conversation to initiate the
// Mosh session.
class SSHLogin {
 public:
  SSHLogin() = default;
  SSHLogin(const SSHLogin&) = delete;
  SSHLogin& operator=(const SSHLogin&) = delete;
  SSHLogin(SSHLogin&&) = default;
  SSHLogin& operator=(SSHLogin&&) = default;
  ~SSHLogin() = default;

  // Begin the SSH login session. Returns true iff SSH Login succeeded.
  bool Start();

  bool use_agent() const { return use_agent_; }
  void set_use_agent(bool use_agent) { use_agent_ = use_agent; }

  Resolver* resolver() const { return resolver_; }
  // Set the resolver to use. Does not take ownership.
  void set_resolver(Resolver* resolver) { resolver_ = resolver; }

  bool trust_sshfp() const { return trust_sshfp_; }
  void set_trust_sshfp(bool trust_sshfp) { trust_sshfp_ = trust_sshfp; }

  std::string host() const { return host_; }
  void set_host(const std::string& host) { host_ = host; }

  Resolver::Type type() const { return type_; }
  void set_type(Resolver::Type type) { type_ = type; }

  std::string port() const { return port_; }
  void set_port(const std::string& port) { port_ = port; }

  std::string user() const { return user_; }
  void set_user(const std::string& user) { user_ = user; }

  std::string key() const { return key_; }
  void set_key(const std::string& key) { key_ = key; }

  // The command to run on the remote host. Set to the empty string to use the
  // default.
  std::string remote_command() const { return remote_command_; }
  void set_remote_command(const std::string& command) {
    remote_command_ = command;
  }

  // The command to start the mosh-server on the remote host. Set to the empty
  // string to use the default.
  std::string server_command() const { return server_command_; }
  void set_server_command(const std::string& command) {
    server_command_ = command;
  }

  pp::VarDictionary known_hosts() const { return known_hosts_; }
  void set_known_hosts(const pp::Var& var) { known_hosts_ = var; }

  std::string mosh_port() const { return mosh_port_; }

  std::string mosh_key() const { return mosh_key_; }

  std::string mosh_addr() const { return mosh_addr_; }

 private:
  // Resolve |host_| and |type_| to |resolved_addr_| and |resolved_fp_| via
  // |resolver_|.
  bool Resolve();

  // Display and check the remote server fingerprint.
  bool CheckFingerprint();

  // Returns the intersection of authentication types that both the client and
  // the server support. Returns nullptr on error.
  std::unique_ptr<std::vector<ssh::AuthenticationType>> GetAuthTypes();

  // Ask a yes/no question to the user, and return the answer as a bool.
  // Prefers to return false if input is not parseable.
  bool AskYesNo(const std::string& prompt);

  bool DoPasswordAuth();
  bool DoInteractiveAuth();
  bool DoPublicKeyAuth();
  bool DoConversation();

  bool use_agent_ = false;
  Resolver* resolver_ = nullptr;
  bool trust_sshfp_ = false;
  std::string host_;
  Resolver::Type type_ = Resolver::Type::A;
  std::string port_;
  std::string user_;
  std::string key_;
  std::string server_command_;
  std::string remote_command_;

  // Resolved address of |host_|.
  std::string resolved_addr_;
  // Resolved fingerprints for |host_|. Empty if none.
  std::vector<std::string> resolved_fp_;

  std::string mosh_port_;
  std::string mosh_key_;
  std::string mosh_addr_;
  pp::VarDictionary known_hosts_;
  std::unique_ptr<ssh::Session> session_;
};

#endif  // SSH_LOGIN_H
