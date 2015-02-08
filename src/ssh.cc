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

#include "ssh.h"

namespace ssh {

using std::string;

KeyboardInteractive::KeyboardInteractive(ssh_session s) : s_(s) {}

KeyboardInteractive::Status KeyboardInteractive::GetStatus() {
  while (true) {
    int result = ssh_userauth_kbdint(s_, nullptr, nullptr);
    if (result == SSH_AUTH_SUCCESS) {
      return kAuthenticated;
    }
    if (result == SSH_AUTH_PARTIAL) {
      return kPartialAuthentication;
    }
    if (result == SSH_AUTH_INFO) {
      current_prompt_ = 0;
      num_prompts_ = ssh_userauth_kbdint_getnprompts(s_);
      if (num_prompts_ == 0) {
        // According to the libssh docs, empty question sets can happen
        // sometimes. Keep calling ssh_userauth_kbdint().
        continue;
      }
      return kPending;
    }
    // We only want to loop back under special situations, so if we got to the
    // bottom, we've done all we can do.
    break;
  }

  return kFailed;
}

string KeyboardInteractive::GetNextPrompt() {
  char echo = 0;
  const char *prompt = ssh_userauth_kbdint_getprompt(
      s_, current_prompt_, &echo);
  echo_answer_ = echo > 0;
  return prompt;
}

bool KeyboardInteractive::Answer(const char *answer) {
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

Session::Session(const string &host, int port, const string &user) :
    s_(ssh_new()) {
  SetOption(SSH_OPTIONS_HOST, host);
  SetOption(SSH_OPTIONS_PORT, port);
  SetOption(SSH_OPTIONS_USER, user);
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
    delete key_;
    key_ = nullptr;
    delete keyboard_interactive_;
    keyboard_interactive_ = nullptr;
    for (::std::vector<Channel *>::iterator it = channels_.begin();
        it != channels_.end();
        ++it) {
      delete *it;
    }
    channels_.clear();
    ssh_disconnect(s_);
  }
}

Key *Session::GetPublicKey() {
  if (connected_ && key_ == nullptr) {
    key_ = new Key();
    ssh_get_publickey(s_, &key_->key_);
  }
  return key_;
}

::std::vector<AuthenticationType> Session::GetAuthenticationTypes() {
  ::std::vector<AuthenticationType> auth_types;

  // First we have to try the "none" method to get the types. This could lead
  // to confusion if it is actually sufficient to authenticate.
  int result = ssh_userauth_none(s_, nullptr);
  if (result == SSH_AUTH_ERROR || result == SSH_AUTH_SUCCESS) {
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
  }
  return "Unknown";
}

KeyboardInteractive *Session::AuthUsingKeyboardInteractive() {
  delete keyboard_interactive_;
  keyboard_interactive_ = new KeyboardInteractive(s_);
  return keyboard_interactive_;
}

bool Session::AuthUsingKey(const Key &key) {
  int result = ssh_userauth_publickey(s_, nullptr, key.key_);
  return ParseCode(result, SSH_AUTH_SUCCESS);
}

Channel *Session::NewChannel() {
  Channel *c = new Channel(ssh_channel_new(s_));
  channels_.push_back(c);
  return c;
}

Key::Key() {}

Key::~Key() {
  if (key_ != nullptr) {
    ssh_key_free(key_);
  }
}

bool Key::ImportPrivateKey(const string &key, const char *passphrase) {
  if (key_ != nullptr) {
    ssh_key_free(key_);
    key_ = nullptr;
  }
  int result = ssh_pki_import_privkey_base64(
      key.c_str(), passphrase, nullptr, nullptr, &key_);
  if (result != SSH_OK) {
    return false;
  }
  return true;
}

Key *Key::GetPublicKey() {
  if (key_ == nullptr) {
    return nullptr;
  }
  ssh_key pubkey;
  int result = ssh_pki_export_privkey_to_pubkey(key_, &pubkey);
  if (result != SSH_OK) {
    return nullptr;
  }
  Key *key = new Key();
  key->key_ = pubkey;
  return key;
}

string Key::MD5() {
  if (key_ == nullptr) {
    return string();
  }
  unsigned char *hash_buf = nullptr;
  size_t hash_len = 0;
  int result = ssh_get_publickey_hash(
      key_, SSH_PUBLICKEY_HASH_MD5, &hash_buf, &hash_len);
  if (result != 0 ) {
    return string();
  }
  char *hash_hex = ssh_get_hexa(hash_buf, hash_len);
  string hash(hash_hex);
  delete hash_hex;
  ssh_clean_pubkey_hash(&hash_buf);
  return hash;
}

Channel::Channel(ssh_channel c) :
    c_(c), session_open_(false) { }

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

bool Channel::Execute(const string &command) {
  if (OpenSession() == false) {
    return false;
  }
  // TODO: Make PTY optional.
  bool result = ParseCode(ssh_channel_request_pty(c_));
  if (result == false) {
    return false;
  }
  return ParseCode(ssh_channel_request_exec(c_, command.c_str()));
}

bool Channel::Read(string *out, string *err) {
  if (session_open_ == false) {
    return false;
  }
  // Nasty hack to avoid compiler warning due to type mismatch in libssh.
  const unsigned int ssh_error = (unsigned int)SSH_ERROR;
  char buffer[256];

  // First read all of stdout.
  unsigned int bytes_read = 1; // Prime the pump.
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
    bytes_read = 1; // Prime the pump.
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

} // namespace ssh
