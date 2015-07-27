// ssh_login.cc - SSH Login for Mosh.

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

#include "ssh_login.h"

#include "make_unique.h"
#include "mosh_nacl.h"

#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using util::make_unique;

const int INPUT_SIZE = 256;
const int RETRIES = 3;
const string kServerCommandDefault(
    "mosh-server new -s -c 256 -l LANG=en_US.UTF-8");

SSHLogin::SSHLogin() {}

SSHLogin::~SSHLogin() {}

void SSHLogin::GetKeyboardLine(char *buf, size_t len, bool echo) {
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
        if (echo == true) {
          // '\b' doesn't "rub out" on its own.
          const char *backspace = "\b\x1b[K";
          write(STDOUT_FILENO, backspace, strlen(backspace));
        }
        --i;
      }
      continue;
    }
    if (echo == true) {
      write(STDOUT_FILENO, &in, 1);
    }
    buf[i] = in;
    ++i;
  }
  buf[i] = 0;
}

bool SSHLogin::AskYesNo(const string &prompt) {
  for (int i = 0; i < RETRIES; ++i) {
    printf("%s (Yes/No): ", prompt.c_str());
    char input_buf[INPUT_SIZE];
    GetKeyboardLine(input_buf, sizeof(input_buf), true);
    printf("\r\n");
    string input = input_buf;
    if (input == "yes" || input == "Yes") {
      return true;
    }
    if (input == "no" || input == "No") {
      return false;
    }
    printf("Please specify Yes or No.\r\n");
  }

  return false;
}

bool SSHLogin::Start() {
  setenv("HOME", "dummy", 1); // To satisfy libssh.

  session_ = make_unique<ssh::Session>(addr_, atoi(port_.c_str()), user_);
  session_->SetOption(SSH_OPTIONS_TIMEOUT, 30); // Extend connection timeout to 30s.

  if (session_->Connect() == false) {
    fprintf(stderr, "Could not connect via ssh: %s\r\n",
        session_->GetLastError().c_str());
    return false;
  }

  if (!CheckFingerprint()) {
    return false;
  }

  const auto auths_ptr = GetAuthTypes();
  if (auths_ptr == nullptr) {
    return false;
  }

  bool authenticated = false;
  for (const auto& auth : *auths_ptr) {
    printf("Trying authentication type %s\r\n",
        ssh::GetAuthenticationTypeName(auth).c_str());

    switch(auth) {
      case ssh::kPassword:
        authenticated = DoPasswordAuth();
        break;
      case ssh::kInteractive:
        authenticated = DoInteractiveAuth();
        break;
      case ssh::kPublicKey:
        authenticated = DoPublicKeyAuth();
        break;
      default:
        // Should not get here.
        assert(false);
    }

    if (authenticated) {
      break;
    }
  }

  // For safety, clear the sensitive data.
  key_.clear();

  if (authenticated == false) {
    fprintf(stderr, "ssh authentication failed: %s\r\n",
        session_->GetLastError().c_str());
    return false;
  }

  if (!DoConversation()) {
    return false;
  }

  return true;
}

bool SSHLogin::CheckFingerprint() {
  string server_name;
  if (addr_.find(':') == string::npos) {
    server_name = addr_ + ":" + port_;
  } else {
    server_name = "[" + addr_ + "]:" + port_;
  }
  const string server_fp = session_->GetPublicKey().MD5();

  printf("Remote ssh host address:\r\n  %s\r\n", server_name.c_str());
  printf("Fingerprint of remote ssh host (MD5):\r\n  %s\r\n",
      server_fp.c_str());

  const pp::Var stored_fp_var = known_hosts_.Get(server_name);
  if (stored_fp_var.is_undefined()) {
    bool result = AskYesNo("Server fingerprint unknown. Store and continue?");
    if (result == true) {
      known_hosts_.Set(server_name, server_fp);
      return true;
    }
  } else {
    string stored_fp = stored_fp_var.AsString();
    if (stored_fp == server_fp) {
      return true;
    }
    printf("WARNING!!! Server fingerprint differs for this host! "
        "Possible man-in-the-middle attack.\r\n"
        "Stored fingerprint (MD5):\r\n  %s\r\n", stored_fp.c_str());
    bool result = AskYesNo("Connect anyway, and store new fingerprint?");
    if (result == true) {
      result = AskYesNo("Don't take this lightly. Are you really sure?");
      if (result == true) {
        known_hosts_.Set(server_name, server_fp);
        return true;
      }
    }
  }

  return false;
}

unique_ptr<vector<ssh::AuthenticationType>> SSHLogin::GetAuthTypes() {
  // Place the list of supported authentications types here, in the order
  // they should be tried.
  vector<ssh::AuthenticationType> client_auths;
  client_auths.push_back(ssh::kPublicKey);
  client_auths.push_back(ssh::kInteractive);
  client_auths.push_back(ssh::kPassword);

  printf("Authentication types supported by server:\r\n");
  vector<ssh::AuthenticationType> server_auths =
    session_->GetAuthenticationTypes();
  if (server_auths.size() == 0) {
    fprintf(stderr, "Failed to get authentication types: %s\r\n",
        session_->GetLastError().c_str());
    return nullptr;
  }

  for (const auto& auth : server_auths) {
    printf(" - %s", ssh::GetAuthenticationTypeName(auth).c_str());
    if (std::find(client_auths.begin(), client_auths.end(), auth) ==
        client_auths.end()) {
      printf(" (not supported by client)");
    }
    printf("\r\n");
  }

  // In order to maintain the order of client_auths, but still display
  // server_auths that are unsupported (above), we have to loop through these
  // again.
  auto supported_auths = make_unique<vector<ssh::AuthenticationType>>();
  for (const auto& auth : client_auths) {
    if (std::find(server_auths.begin(), server_auths.end(), auth) !=
        server_auths.end()) {
      supported_auths->push_back(auth);
    }
  }

  return supported_auths;
}

bool SSHLogin::DoPasswordAuth() {
  for (int tries = RETRIES; tries > 0; --tries) {
    char input[INPUT_SIZE];
    printf("Password: ");
    GetKeyboardLine(input, sizeof(input), false);
    printf("\r\n");
    if (strlen(input) == 0) {
      // User provided no input; skip this authentication type.
      return false;
    }
    bool authenticated = session_->AuthUsingPassword(input);
    // For safety, zero the sensitive input ASAP.
    memset(input, 0, sizeof(input));
    if (authenticated) {
      return true;
    }
    if (tries == 1) {
      // Only display error on last try.
      fprintf(stderr, "Password authentication failed: %s\r\n",
          session_->GetLastError().c_str());
    }
  }
  return false;
}

bool SSHLogin::DoInteractiveAuth() {
  ssh::KeyboardInteractive& kbd = session_->AuthUsingKeyboardInteractive();

  for (int tries = RETRIES; tries > 0; --tries) {
    ssh::KeyboardInteractive::Status status = kbd.GetStatus();
    while (status == ssh::KeyboardInteractive::kPending) {
      if (kbd.GetName().size() > 0) {
        printf("%s\r\n", kbd.GetName().c_str());
      }
      if (kbd.GetInstruction().size() > 0) {
        printf("%s\r\n", kbd.GetInstruction().c_str());
      }
      bool done = false;
      while (!done) {
        char input[INPUT_SIZE];
        printf("%s", kbd.GetNextPrompt().c_str());
        GetKeyboardLine(input, sizeof(input), kbd.IsAnswerEchoed());
        printf("\r\n");
        if (strlen(input) == 0) {
          // User provided no input; skip this authentication type.
          status = ssh::KeyboardInteractive::kFailed;
          tries = 0;
          break;
        }
        done = kbd.Answer(input);
        // For safety, zero the sensitive input ASAP.
        memset(input, 0, sizeof(input));
      }
      status = kbd.GetStatus();
    }
    const char *error = nullptr;
    switch (status) {
      case ssh::KeyboardInteractive::kAuthenticated:
        return true;
      case ssh::KeyboardInteractive::kPartialAuthentication:
        error = "Keyboard interactive succeeded but insufficient.";
        tries = 0;
        break;
      case ssh::KeyboardInteractive::kFailed: // fallthrough
      default:
        error = "Keyboard interactive auth failed.";
        break;
    }
    if (error != nullptr && tries == 1) {
      // Only display error on the last try.
      fprintf(stderr, "%s\r\n", error);
    }
  }
  return false;
}

bool SSHLogin::DoPublicKeyAuth() {
  for (int tries = RETRIES; tries > 0; --tries) {
    if (key_.size() == 0) {
      printf("No ssh key found.\r\n");
      return false;
    }
    // First see if key loads with no passphrase.
    ssh::Key key;
    if (!key.ImportPrivateKey(key_, "")) {
      printf("Passphrase: ");
      char input[INPUT_SIZE];
      GetKeyboardLine(input, sizeof(input), false);
      printf("\r\n");
      if (strlen(input) == 0) {
        // User provided no input; skip this authentication type.
        return false;
      }
      bool result = key.ImportPrivateKey(key_, input);
      memset(input, 0, sizeof(input));
      if (result == false) {
        if (tries == 1) {
          // Only display error on the last try.
          fprintf(stderr, "Error reading key. This could be due to the wrong "
              "passphrase, the key type being unsupported, or the key format "
              "being incorrect or corrupt.\r\n");
        }
        continue;
      }
    }
    if (session_->AuthUsingKey(key) == false) {
      fprintf(stderr, "Key auth failed: %s\r\n",
          session_->GetLastError().c_str());
      return false;
    }
    // If we got here, auth succeeded.
    return true;
  }
  return false;
}

bool SSHLogin::DoConversation() {
  string command;

  if (server_command().size() > 0) {
    command = server_command();
  } else {
    command = kServerCommandDefault;
  }

  if (remote_command().size() > 0) {
    command += " -- " + remote_command();
  }

  ssh::Channel& c = session_->NewChannel();
  if (c.Execute(command) == false) {
    fprintf(stderr, "Failed to execute mosh-server: %s\r\n",
        session_->GetLastError().c_str());
    return false;
  }
  string buf;
  if (c.Read(&buf, nullptr) == false) {
    fprintf(stderr, "Error reading from remote ssh server: %s\r\n",
        session_->GetLastError().c_str());
    return false;
  }

  // Default Mosh address to the one with which we connected. It
  // will get overridden if there's a MOSH IP line in the
  // response.
  mosh_addr_ = addr_;

  size_t left_pos = 0;
  while (true) {
    const char *newline = "\r\n";
    size_t right_pos = buf.find(newline, left_pos);
    if (right_pos == string::npos) {
      break;
    }
    string substr = buf.substr(left_pos, right_pos - left_pos);
    if (substr.find("MOSH CONNECT") == 0) {  // Found at beginning of line.
      char key[23];
      char port[6];
      if (sscanf(substr.c_str(), "MOSH CONNECT %5s %22s", port, key) != 2) {
        fprintf(stderr, "Badly formatted MOSH CONNECT line: %s\r\n",
            substr.c_str());
        return false;
      }
      mosh_key_ = key;
      mosh_port_ = port;
    } else if (substr.find("MOSH IP") == 0) {  // Found at beginning of line.
      char addr[64];
      if (sscanf(substr.c_str(), "MOSH IP %63s", addr) != 1) {
        fprintf(stderr, "Badly formatted MOSH IP line: %s\r\n", substr.c_str());
        return false;
      }
      mosh_addr_ = addr;
    }
    left_pos = right_pos + strlen(newline);
  }

  if (mosh_key_.size() == 0 || mosh_port_.size() == 0) {
    fprintf(stderr, "Bad response when running mosh-server: '%s'\r\n",
        buf.c_str());
    return false;
  }
  return true;

}
