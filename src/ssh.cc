// ssh.cc - C++ wrapper around libssh.

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

#include "ssh.h"

namespace ssh {

Session::Session(const string &host, int port, const string &user) :
    s_(ssh_new()), connected_(false), key_(NULL) {
  SetOption(SSH_OPTIONS_HOST, host);
  SetOption(SSH_OPTIONS_PORT, port);
  SetOption(SSH_OPTIONS_USER, user);
}

Session::~Session() {
  Disconnect();
  ssh_free(s_);
}

bool Session::Connect() {
  int result = ssh_connect(s_);
  if (result == SSH_OK) {
    connected_ = true;
  }
  return ParseCode(result);
}

void Session::Disconnect() {
  if (connected_) {
    connected_ = false;
    if (key_ != NULL) {
      delete key_;
      key_ = NULL;
    }
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
  if (connected_ && key_ == NULL) {
    ssh_key key = NULL;
    ssh_get_publickey(s_, &key);
    key_ = new Key(key);
  }
  return key_;
}

Channel *Session::NewChannel() {
  Channel *c = new Channel(ssh_channel_new(s_));
  channels_.push_back(c);
  return c;
}

Key::Key(ssh_key key) : key_(key) { }

Key::~Key() {
  ssh_key_free(key_);
}

string Key::MD5() {
  if (key_ == NULL) {
    return string();
  }
  unsigned char *hash_buf = NULL;
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
  if (out != NULL) {
    while (bytes_read > 0 && bytes_read != ssh_error) {
      bytes_read = ssh_channel_read(c_, buffer, sizeof(buffer), 0);
      out->append(buffer, bytes_read);
    }
    if (bytes_read == ssh_error) {
      return false;
    }
  }

  // Now read all of stderr.
  if (err != NULL) {
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
