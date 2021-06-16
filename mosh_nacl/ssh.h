// ssh.h - C++ wrapper around libssh.
//
// There is already a thin wrapper available in "libssh/libsshpp.hpp", but it
// is clunky, and doesn't provide access to ssh_get_pubkey_hash() nor
// ssh_get_publickey() in any way. This wrapper aims to be better, but will be
// a subset of the functionality needed for the immediate purposes.

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

#ifndef MOSH_NACL_SSH_H_
#define MOSH_NACL_SSH_H_

#include <libssh/libssh.h>

#include <memory>
#include <string>
#include <vector>

namespace ssh {

class Key;
class Channel;

// Base class implementing shared error handling code.
class ResultCode {
 public:
  // Get the error code from the last call. This code is what the underlyling
  // libssh call returns (e.g., SSH_OK, SSH_ERORR, etc.).
  int GetLastErrorCode() const { return last_code_; }

 protected:
  // Converts return codes into simple error handling.
  bool ParseCode(int code, int ok = SSH_OK) {
    last_code_ = code;
    return code == ok;
  }

 private:
  int last_code_ = SSH_OK;
};

// Types of authentications. This maps to libssh's bitfield macros as noted.
enum class AuthenticationType {
  kPassword = 0,  // SSH_AUTH_METHOD_PASSWORD
  kPublicKey,     // SSH_AUTH_METHOD_PUBLICKEY
  kHostBased,     // SSH_AUTH_METHOD_HOSTBASED
  kInteractive,   // SSH_AUTH_METHOD_INTERACTIVE
  kNone,          // SSH_AUTH_METHOD_NONE
};

// Get a human-readable text representation of an authentication type.
std::string GetAuthenticationTypeName(AuthenticationType type);

// Represents a keyboard-interactive authorization "subsession". Should only be
// gotten from Session::AuthUsingKeyboardInteractive().
//
// Usage: In a loop, call GetStatus() while it returns kPending. In there, form
// a loop calling GetNextPrompt() and Answer(). Answer will return false until
// all answers are given, at which point the loop should go back to
// GetStatus(). If GetStatus() returns kAuthenticated, you are done. If it
// returns kPartialAuthentication, you must continue with another
// authentication method. If it returns kFailed, this authentication failed
// completely (but others may succeed).
class KeyboardInteractive {
 public:
  enum Status {
    kAuthenticated = 0,      // Authentication fully successful.
    kPartialAuthentication,  // OK, but more auth methods required.
    kPending,                // Pending more prompts and answers.
    kFailed,                 // Authentication failed.
  };

  KeyboardInteractive() = delete;
  explicit KeyboardInteractive(ssh_session s);
  KeyboardInteractive(const KeyboardInteractive&) = delete;
  KeyboardInteractive& operator=(const KeyboardInteractive&) = delete;
  ~KeyboardInteractive() = default;

  // Returns the current status of keyboard-interactive auth.
  Status GetStatus();

  // Returns the "name" string from the server. Must have called
  // GetStatus() first.
  std::string GetName() { return std::string(ssh_userauth_kbdint_getname(s_)); }

  // Returns the "instruction" string from the server. Must have called
  // GetStatus() first.
  std::string GetInstruction() const { return instruction_; }

  // Returns the next prompt from the server.
  std::string GetNextPrompt();

  // For the current prompt, indicate whether the answer should be echoed to
  // the user. Should be called after GetNextPrompt(); behavior is undefined
  // otherwise.
  // To be clear: This class never prints anything; it is the responsibility of
  // the caller to implement echoing.
  bool IsAnswerEchoed() const { return echo_answer_; }

  // Answer the most recently gotten prompt. Returns true if authentication is
  // complete; otherwise, call GetNextPrompt() to keep going.
  //
  // Using a plain char * to allow you to manage the lifecycle of sensitive
  // data. This class does not make a copy of the string, but just passes it to
  // libssh.
  bool Answer(const char* answer);

 private:
  ssh_session const s_;
  int num_prompts_ = 0;
  int current_prompt_ = 0;
  bool echo_answer_ = false;
  std::string instruction_;
};

// Represents an ssh session.
class Session : public ResultCode {
 public:
  Session() = delete;
  Session(const std::string& host, int port, const std::string& user);
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session();

  // Gets the human-readable error string from the last call. Analog to
  // ssh_get_error().
  std::string GetLastError() { return std::string(ssh_get_error(s_)); }

  // Connect to the host. Analog to ssh_connect().
  bool Connect();

  // Disconnect from the host. This is not necessary to call unless you want to
  // reuse the object, as it is called from the destructor. Analog to
  // ssh_disconnect().
  void Disconnect();

  // Determines if the connected server is known. Analog to
  // ssh_session_is_known_server().
  bool ServerKnown() {
    return ParseCode(ssh_session_is_known_server(s_), SSH_SERVER_KNOWN_OK);
  }

  // Returns the public key as a Key. Ownership is retained, thus is valid only
  // for the lifetime of Session. Analog to ssh_get_server_publickey().
  Key& GetPublicKey();

  // Get a list of authentication types available. On error, or if the server
  // is stubborn, the list will be empty. Check GetLastError().
  ::std::vector<AuthenticationType> GetAuthenticationTypes();

  // Authenticate using password auth. Analog to ssh_userauth_password().
  //
  // Using a plain char * to allow you to manage the lifecycle of sensitive
  // data. This class does not make a copy of the string, but just passes it to
  // libssh.
  bool AuthUsingPassword(const char* password) {
    return ParseCode(ssh_userauth_password(s_, nullptr, password),
                     SSH_AUTH_SUCCESS);
  }

  // Authenticate using keyboard-interactive auth. Returns a
  // KeyboardInteractive object (see documentation for that class). Session
  // retains ownership of this object, and it is only valid for the lifetime of
  // Session. If you've already gotten a KeyboardInteractive, the old one is
  // unusable after this method is called.
  KeyboardInteractive& AuthUsingKeyboardInteractive();

  // Auth using a private key. See class Key for how to prepare this object.
  bool AuthUsingKey(const Key& key);

  // Auth using an SSH agent (set environment variable SSH_AUTH_SOCK).
  bool AuthUsingAgent();

  // Gets a new channel. Ownership is retained, thus is valid only for the
  // lifetime of Session. Analog to ssh_channel_new().
  Channel& NewChannel();

  // Analog to ssh_options_set(), but easier to use as it is overloaded to
  // handle the various input types.
  bool SetOption(enum ssh_options_e type, const std::string& option) {
    return ParseCode(ssh_options_set(s_, type, option.c_str()));
  }
  bool SetOption(enum ssh_options_e type, const char* option) {
    return ParseCode(ssh_options_set(s_, type, option));
  }
  // Using "long int" because that's how libssh defines it.
  bool SetOption(enum ssh_options_e type,
                 long int option) {  // NOLINT(runtime/int)
    return ParseCode(ssh_options_set(s_, type, &option));
  }
  bool SetOption(enum ssh_options_e type, void* option) {
    return ParseCode(ssh_options_set(s_, type, option));
  }

 private:
  ssh_session s_ = nullptr;
  bool connected_ = false;
  std::string user_;
  std::unique_ptr<Key> key_;
  std::vector<std::unique_ptr<Channel>> channels_;
  std::unique_ptr<KeyboardInteractive> keyboard_interactive_;
};

// Represents a type of ssh key. This is a simple value type.
class KeyType {
 public:
  enum KeyTypeEnum {
    UNKNOWN = 0,
    DSS,
    RSA,
    RSA1,
    ECDSA,
    ED25519,
    DSS_CERT00,
    RSA_CERT00,
    DSS_CERT01,
    RSA_CERT01,
    ECDSA_P256,
    ECDSA_P384,
    ECDSA_P521,
    ECDSA_P256_CERT01,
    ECDSA_P384_CERT01,
    ECDSA_P521_CERT01,
    ED25519_CERT01,
  };

  KeyType() = default;
  KeyType(const KeyType&) = default;
  KeyType& operator=(const KeyType&) = default;
  ~KeyType() = default;

  explicit KeyType(KeyTypeEnum type);

  KeyType::KeyTypeEnum type() const;

  // Get a human-readable string representation of the key type.
  std::string AsString() const;

 private:
  friend class Key;
  explicit KeyType(ssh_keytypes_e type) : type_(type) {}

  ssh_keytypes_e type_ = SSH_KEYTYPE_UNKNOWN;
};

// Represents a key.
class Key {
  friend class Session;

 public:
  Key();
  Key(const Key&) = delete;
  Key& operator=(const Key&) = delete;
  Key(Key&&) = default;
  Key& operator=(Key&&) = default;
  ~Key();

  // Import a base64 formatted private key. If no passphrase is required, pass
  // nullptr; if the key was encrypted, the method will return false, and you
  // can prompt the user for a passphrase and try again. Using char * to better
  // manage lifecycle of sensitive data.
  bool ImportPrivateKey(const std::string& key, const char* passphrase);

  // Import a base64 formatted public key.
  bool ImportPublicKey(const std::string& key, KeyType type);

  // Get the public version of the private key. Only works if a private key is
  // loaded into the current object. Ownership is transferred to the caller.
  // Returns nullptr on error.
  std::unique_ptr<Key> GetPublicKey() const;

  // Get key as MD5 hash. Will return an empty string on error.
  std::string MD5() const;

  // Get key as SHA1 hash. Will return an empty string on error.
  std::string SHA1() const;

  // Get key as SHA256 hash. Will return an empty string on error.
  std::string SHA256() const;

  // Get the key type of this key.
  KeyType GetKeyType() const;

 private:
  std::string Hash(ssh_publickey_hash_type type) const;

  ssh_key key_ = nullptr;
};

// Represents an ssh channel.
class Channel : public ResultCode {
 public:
  friend class Session;

  Channel() = delete;
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  ~Channel();

  // Execute the command. Analog to ssh_channel_request_exec().
  bool Execute(const std::string& command);

  // Read the whole stdout/stderr contents from the remote side. Bring your
  // own strings. Set to nullptr if you don't care about one or the other.
  bool Read(std::string* out, std::string* err);

 private:
  explicit Channel(ssh_channel c);

  // Opens a session. This is private because it is handled automatically, and
  // should never need to be called by the user.  Analog to
  // ssh_channel_open_session(), but that shouldn't matter to you.
  bool OpenSession();

  // Close the channel. This is private because it is handled automatically,
  // and should never need to be called by the user. Analog to
  // ssh_channel_close().
  bool Close();

  ssh_channel c_ = nullptr;
  // Whether a session has been opened.
  bool session_open_ = false;
};

}  // namespace ssh

#endif  // MOSH_NACL_SSH_H_
