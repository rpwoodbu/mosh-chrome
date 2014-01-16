// ssh.h - C++ wrapper around libssh.
//
// There is already a thin wrapper available in "libssh/libsshpp.hpp", but it
// is clunky, and doesn't provide access to ssh_get_pubkey_hash() nor
// ssh_get_publickey() in any way. This wrapper aims to be better, but will be
// a subset of the functionality needed for the immediate purposes.

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

#include <libssh/libssh.h>
#include <string>
#include <vector>

namespace ssh {

using ::std::string;

class Key;
class Channel;

// Base class implementing shared error handling code.
class ResultCode {
 public:
  ResultCode() : last_code_(SSH_OK) {}

  // Get the error code from the last call. This code is what the underlyling
  // libssh call returns (e.g., SSH_OK, SSH_ERORR, etc.).
  int GetLastErrorCode() { return last_code_; }

 protected:
  // Converts return codes into simple error handling.
  bool ParseCode(int code, int ok=SSH_OK) {
    last_code_ = code;
    return code == ok;
  }

 private:
  int last_code_;
};

// Represents an ssh session.
class Session : public ResultCode {
 public:
  Session(const string &host, int port, const string &user);
  ~Session();

  // Gets the human-readable error string from the last call. Analog to
  // ssh_get_error().
  string GetLastError() { return string(ssh_get_error(s_)); }

  // Connect to the host. Analog to ssh_connect().
  bool Connect();

  // Disconnect from the host. This is not necessary to call unless you want to
  // reuse the object, as it is called from the destructor. Analog to
  // ssh_disconnect().
  void Disconnect();

  // Determines if the connected server is known. Analog to
  // ssh_is_server_known().
  bool ServerKnown() {
    return ParseCode(ssh_is_server_known(s_), SSH_SERVER_KNOWN_OK);
  }

  // Returns the public key as a Key. Ownership is retained, thus is valid only
  // for the lifetime of Session. Analog to ssh_get_publickey().
  Key *GetPublicKey();

  // Authenticate using password auth. Analog to ssh_userauth_password().
  bool AuthUsingPassword(const string &password) {
    return ParseCode(
      ssh_userauth_password(s_, NULL, password.c_str()), SSH_AUTH_SUCCESS);
  }

  // Gets a new channel. Ownership is retained, thus is valid only for the
  // lifetime of Session. Analog to ssh_channel_new().
  Channel *NewChannel();

  // Analog to ssh_options_set(), but easier to use as it is overloaded to
  // handle the various input types.
  bool SetOption(enum ssh_options_e type, const string &option) {
    return ParseCode(ssh_options_set(s_, type, option.c_str()));
  }
  bool SetOption(enum ssh_options_e type, const char *option) {
    return ParseCode(ssh_options_set(s_, type, option));
  }
  bool SetOption(enum ssh_options_e type, long int option) {
    return ParseCode(ssh_options_set(s_, type, &option));
  }
  bool SetOption(enum ssh_options_e type, void *option) {
    return ParseCode(ssh_options_set(s_, type, option));
  }

 private:
  ssh_session s_;
  bool connected_;
  Key *key_;
  ::std::vector<Channel *> channels_;

  // Disable copy and assign.
  Session(Session &);
  Session &operator=(Session &);
};

// Represents a key. Do not instantiate directly; call Session::GetPublicKey().
class Key {
 public:
  // Do not call this constructor; use Session::GetPublicKey().
  explicit Key(ssh_key key);
  ~Key();

  // Get key as MD5 hash. Will return an empty string on error.
  string MD5();

 private:
  ssh_key key_;

  // Disable copy and assign.
  Key(Key &);
  Key &operator=(Key &);
};

// Represents an ssh channel. Do not instantiate this yourself; should be
// obtained via Session::NewChannel().
class Channel : public ResultCode {
 public:
  // Do not call this constructor; use Session::NewChannel().
  explicit Channel(ssh_channel c);
  ~Channel();

  // Execute the command. Analog to ssh_channel_request_exec().
  bool Execute(const string &command);

  // Read the whole stdout/stderr contents from the remote side. Bring your
  // own strings. Set to NULL if you don't care about one or the other.
  bool Read(string *out, string *err);

 private:
  // Opens a session. This is private because it is handled automatically, and
  // should never need to be called by the user.  Analog to
  // ssh_channel_open_session(), but that shouldn't matter to you.
  bool OpenSession();

  // Close the channel. This is private because it is handled automatically,
  // and should never need to be called by the user. Analog to
  // ssh_channel_close().
  bool Close();

  ssh_channel c_;
  // Whether a session has been opened.
  bool session_open_;

  // Disable copy and assign.
  Channel(Channel &);
  Channel &operator=(Channel &);
};

} // namespace ssh
