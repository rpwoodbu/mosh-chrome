// ssh.cc - C++ wrapper around libssh.

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

#include "mosh_nacl/ssh.h"

#include <utility>

#include "mosh_nacl/make_unique.h"

namespace ssh {

using std::string;
using std::unique_ptr;
using util::make_unique;

KeyboardInteractive::KeyboardInteractive(ssh_session const s) : s_(s) {}

KeyboardInteractive::Status KeyboardInteractive::GetStatus() {
  while (true) {
    int result = ssh_userauth_kbdint(s_, nullptr, nullptr);
    switch (result) {
      case SSH_AUTH_SUCCESS:
        return kAuthenticated;
      case SSH_AUTH_PARTIAL:
        return kPartialAuthentication;
      case SSH_AUTH_DENIED:
        return kFailed;

      case SSH_AUTH_INFO:
        instruction_ = string(ssh_userauth_kbdint_getinstruction(s_));
        current_prompt_ = 0;
        num_prompts_ = ssh_userauth_kbdint_getnprompts(s_);
        if (num_prompts_ == 0) {
          // According to the libssh docs, empty question sets can happen
          // sometimes. Keep calling ssh_userauth_kbdint().
          continue;
        }
        return kPending;

      default:
        // We only want to loop back under special situations, so if we got an
        // unexpected response, there's nothing left to do.
        break;
    }
  }

  return kFailed;
}

string KeyboardInteractive::GetNextPrompt() {
  char echo = 0;
  const char* prompt =
      ssh_userauth_kbdint_getprompt(s_, current_prompt_, &echo);
  echo_answer_ = echo > 0;
  return prompt;
}

bool KeyboardInteractive::Answer(const char* answer) {
  int result = ssh_userauth_kbdint_setanswer(s_, current_prompt_, answer);
  if (result < 0) {
    return false;
  }
  ++current_prompt_;
  if (current_prompt_ < num_prompts_) {
    return false;
  }
  return true;
}

Session::Session(const string& host, int port, const string& user)
    : s_(ssh_new()), user_(user) {
  SetOption(SSH_OPTIONS_HOST, host);
  SetOption(SSH_OPTIONS_PORT, port);
  SetOption(SSH_OPTIONS_USER, user);

  // libssh 0.7.1 seems to be unable to verify ed25519 host keys, and causes
  // the connection to hosts with such a key to fail. This works around the
  // issue by removing ed25519 from the list of host keys libssh will prefer.
  // This list is from libssh's HOSTKEYS (kex.c), with ssh-ed25519 removed.
  //
  // TODO(rpwoodbu): Eliminate this workaround once ed25519 host key
  // verification is fixed.
  SetOption(SSH_OPTIONS_HOSTKEYS,
            "ecdsa-sha2-nistp256,ecdsa-sha2-nistp384,ecdsa-sha2-nistp521,"
            "ssh-rsa,ssh-dss");
}

Session::~Session() {
  Disconnect();
  ssh_free(s_);
}

bool Session::Connect() {
  if (connected_) {
    Disconnect();
  }
  int result = ssh_connect(s_);
  if (result == SSH_OK) {
    connected_ = true;
  }
  return ParseCode(result);
}

void Session::Disconnect() {
  if (connected_) {
    connected_ = false;
    key_.reset();
    keyboard_interactive_.reset();
    channels_.clear();
    ssh_disconnect(s_);
  }
}

Key& Session::GetPublicKey() {
  if (connected_ && key_ == nullptr) {
    key_ = make_unique<Key>();
    ssh_get_publickey(s_, &key_->key_);
  }
  return *key_;
}

::std::vector<AuthenticationType> Session::GetAuthenticationTypes() {
  ::std::vector<AuthenticationType> auth_types;

  // First we have to try the "none" method to get the types. If it succeeds,
  // then we're in, and there's no reason to list other auth types; just return
  // kNone.
  int result = ssh_userauth_none(s_, nullptr);
  if (result == SSH_AUTH_SUCCESS) {
    auth_types.push_back(kNone);
    return auth_types;
  }
  if (result == SSH_AUTH_ERROR) {
    ParseCode(result);
    // Returning an empty list.
    return auth_types;
  }

  int auth_list = ssh_userauth_list(s_, nullptr);
  if (auth_list & SSH_AUTH_METHOD_PASSWORD) {
    auth_types.push_back(kPassword);
  }
  if (auth_list & SSH_AUTH_METHOD_PUBLICKEY) {
    auth_types.push_back(kPublicKey);
  }
  if (auth_list & SSH_AUTH_METHOD_HOSTBASED) {
    auth_types.push_back(kHostBased);
  }
  if (auth_list & SSH_AUTH_METHOD_INTERACTIVE) {
    auth_types.push_back(kInteractive);
  }

  return auth_types;
}

string GetAuthenticationTypeName(AuthenticationType type) {
  switch (type) {
    case ssh::kPassword:
      return "Password";
    case ssh::kPublicKey:
      return "Public Key";
    case ssh::kHostBased:
      return "Host Based";
    case ssh::kInteractive:
      return "Keyboard Interactive";
    case ssh::kNone:
      return "None";
  }
  // No default nor return; compiler will complain about missing enum.
}

KeyboardInteractive& Session::AuthUsingKeyboardInteractive() {
  keyboard_interactive_ = make_unique<KeyboardInteractive>(s_);
  return *keyboard_interactive_;
}

bool Session::AuthUsingKey(const Key& key) {
  int result = ssh_userauth_publickey(s_, nullptr, key.key_);
  return ParseCode(result, SSH_AUTH_SUCCESS);
}

bool Session::AuthUsingAgent() {
  int result = ssh_userauth_agent(s_, user_.c_str());
  return ParseCode(result, SSH_AUTH_SUCCESS);
}

Channel& Session::NewChannel() {
  // Can't use make_unique<>() because of private ctor.
  auto c = unique_ptr<Channel>(new Channel(ssh_channel_new(s_)));
  auto& ref = *c;
  channels_.push_back(move(c));
  return ref;
}

KeyType::KeyType(KeyTypeEnum type) {
  switch (type) {
    case DSS:
      type_ = SSH_KEYTYPE_DSS;
      break;
    case RSA:
      type_ = SSH_KEYTYPE_RSA;
      break;
    case RSA1:
      type_ = SSH_KEYTYPE_RSA1;
      break;
    case ECDSA:
      type_ = SSH_KEYTYPE_ECDSA;
      break;
    case ED25519:
      type_ = SSH_KEYTYPE_ED25519;
      break;
    case DSS_CERT00:
      type_ = SSH_KEYTYPE_DSS_CERT00;
      break;
    case RSA_CERT00:
      type_ = SSH_KEYTYPE_RSA_CERT00;
      break;
    case DSS_CERT01:
      type_ = SSH_KEYTYPE_DSS_CERT01;
      break;
    case RSA_CERT01:
      type_ = SSH_KEYTYPE_RSA_CERT01;
      break;
    case ECDSA_SHA2_NISTP256_CERT01:
      type_ = SSH_KEYTYPE_ECDSA_SHA2_NISTP256_CERT01;
      break;
    case ECDSA_SHA2_NISTP384_CERT01:
      type_ = SSH_KEYTYPE_ECDSA_SHA2_NISTP384_CERT01;
      break;
    case ECDSA_SHA2_NISTP521_CERT01:
      type_ = SSH_KEYTYPE_ECDSA_SHA2_NISTP521_CERT01;
      break;

    case UNKNOWN:  // Fallthrough.
    default:
      type_ = SSH_KEYTYPE_UNKNOWN;
      break;
  }
}

KeyType::KeyTypeEnum KeyType::type() const {
  switch (type_) {
    case SSH_KEYTYPE_DSS:
      return DSS;
    case SSH_KEYTYPE_RSA:
      return RSA;
    case SSH_KEYTYPE_RSA1:
      return RSA1;
    case SSH_KEYTYPE_ECDSA:
      return ECDSA;
    case SSH_KEYTYPE_ED25519:
      return ED25519;
    case SSH_KEYTYPE_DSS_CERT00:
      return DSS_CERT00;
    case SSH_KEYTYPE_RSA_CERT00:
      return RSA_CERT00;
    case SSH_KEYTYPE_DSS_CERT01:
      return DSS_CERT01;
    case SSH_KEYTYPE_RSA_CERT01:
      return RSA_CERT01;
    case SSH_KEYTYPE_ECDSA_SHA2_NISTP256_CERT01:
      return ECDSA_SHA2_NISTP256_CERT01;
    case SSH_KEYTYPE_ECDSA_SHA2_NISTP384_CERT01:
      return ECDSA_SHA2_NISTP384_CERT01;
    case SSH_KEYTYPE_ECDSA_SHA2_NISTP521_CERT01:
      return ECDSA_SHA2_NISTP521_CERT01;
    case SSH_KEYTYPE_UNKNOWN:
      return UNKNOWN;
  }
#ifndef __clang__
  // Should be unreachable, but GCC doesn't understand that all enum cases
  // above are covered. GCC is useful for unit tests, as it is usually the
  // compiler installed by default. Prefer to keep this hack out of Clang so it
  // can detect any missing enums.
  return UNKNOWN;
#endif
}

string KeyType::AsString() const { return string(ssh_key_type_to_char(type_)); }

Key::Key() {}

Key::~Key() {
  if (key_ != nullptr) {
    ssh_key_free(key_);
  }
}

bool Key::ImportPrivateKey(const string& key, const char* passphrase) {
  if (key_ != nullptr) {
    ssh_key_free(key_);
    key_ = nullptr;
  }
  int result = ssh_pki_import_privkey_base64(key.c_str(), passphrase, nullptr,
                                             nullptr, &key_);
  if (result != SSH_OK) {
    return false;
  }
  return true;
}

bool Key::ImportPublicKey(const string& key, KeyType type) {
  if (key_ != nullptr) {
    ssh_key_free(key_);
    key_ = nullptr;
  }
  int result = ssh_pki_import_pubkey_base64(key.c_str(), type.type_, &key_);
  if (result != SSH_OK) {
    return false;
  }
  return true;
}

unique_ptr<Key> Key::GetPublicKey() const {
  if (key_ == nullptr) {
    return nullptr;
  }
  ssh_key pubkey;
  int result = ssh_pki_export_privkey_to_pubkey(key_, &pubkey);
  if (result != SSH_OK) {
    return nullptr;
  }
  auto key = make_unique<Key>();
  key->key_ = pubkey;
  return key;
}

string Key::MD5() const { return Hash(SSH_PUBLICKEY_HASH_MD5); }

string Key::SHA1() const { return Hash(SSH_PUBLICKEY_HASH_SHA1); }

string Key::SHA256() const { return Hash(SSH_PUBLICKEY_HASH_SHA256); }

string Key::Hash(const ssh_publickey_hash_type type) const {
  if (key_ == nullptr) {
    return string();
  }
  unsigned char* hash_buf = nullptr;
  size_t hash_len = 0;
  int result = ssh_get_publickey_hash(key_, type, &hash_buf, &hash_len);
  if (result != 0) {
    return string();
  }
  unique_ptr<char[]> hash_hex(ssh_get_hexa(hash_buf, hash_len));
  string hash(hash_hex.get());
  ssh_clean_pubkey_hash(&hash_buf);
  return hash;
}

KeyType Key::GetKeyType() const { return KeyType(ssh_key_type(key_)); }

Channel::Channel(ssh_channel c) : c_(c) {}

Channel::~Channel() {
  Close();
  ssh_channel_free(c_);
}

bool Channel::Close() {
  if (session_open_) {
    if (ParseCode(ssh_channel_close(c_)) == false) {
      return false;
    }
    session_open_ = false;
  }
  return true;
}

bool Channel::Execute(const string& command) {
  if (OpenSession() == false) {
    return false;
  }
  // TODO(rpwoodbu): Make PTY optional.
  bool result = ParseCode(ssh_channel_request_pty(c_));
  if (result == false) {
    return false;
  }
  return ParseCode(ssh_channel_request_exec(c_, command.c_str()));
}

bool Channel::Read(string* out, string* err) {
  if (session_open_ == false) {
    return false;
  }
  // Nasty hack to avoid compiler warning due to type mismatch in libssh.
  const unsigned int ssh_error = (unsigned int)SSH_ERROR;
  char buffer[256];

  // First read all of stdout.
  unsigned int bytes_read = 1;  // Prime the pump.
  if (out != nullptr) {
    while (bytes_read > 0 && bytes_read != ssh_error) {
      bytes_read = ssh_channel_read(c_, buffer, sizeof(buffer), 0);
      out->append(buffer, bytes_read);
    }
    if (bytes_read == ssh_error) {
      return false;
    }
  }

  // Now read all of stderr.
  if (err != nullptr) {
    bytes_read = 1;  // Prime the pump.
    while (bytes_read > 0 && bytes_read != ssh_error) {
      bytes_read = ssh_channel_read(c_, buffer, sizeof(buffer), 1);
      err->append(buffer, bytes_read);
    }
    if (bytes_read == ssh_error) {
      return false;
    }
  }

  return true;
}

bool Channel::OpenSession() {
  if (session_open_ == false) {
    if (ParseCode(ssh_channel_open_session(c_)) == false) {
      return false;
    }
    session_open_ = true;
  }
  return true;
}

}  // namespace ssh
