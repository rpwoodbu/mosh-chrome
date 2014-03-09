// ssh_login.cc - SSH Login for Mosh.
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

#include "ssh_login.h"

#include "mosh_nacl.h"

const int INPUT_SIZE = 256;
const int RETRIES = 3;

SSHLogin::SSHLogin(MoshClientInstance *mosh) : mosh_(mosh), session_(NULL) {}

SSHLogin::~SSHLogin() {
  delete session_;
}

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
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, prompt + " (Yes/No): ");
    char input_buf[INPUT_SIZE];
    GetKeyboardLine(input_buf, sizeof(input_buf), true);
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "\r\n");
    string input = input_buf;
    if (input == "yes" || input == "Yes") {
      return true;
    }
    if (input == "no" || input == "No") {
      return false;
    }
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
        "Please specify Yes or No.\r\n");
  }

  return false;
}

bool SSHLogin::Start() {
  setenv("HOME", "dummy", 1); // To satisfy libssh.

  delete session_;
  session_ = new ssh::Session(addr_, atoi(port_.c_str()), user_);
  session_->SetOption(SSH_OPTIONS_TIMEOUT, 30); // Extend connection timeout to 30s.

  if (session_->Connect() == false) {
    mosh_->Error("Could not connect via ssh: %s",
        session_->GetLastError().c_str());
    return false;
  }

  if (!CheckFingerprint()) {
    return false;
  }

  vector<ssh::AuthenticationType> *auths = GetAuthTypes();
  if (auths == NULL) {
    return false;
  }

  bool authenticated = false;
  for (vector<ssh::AuthenticationType>::iterator i = auths->begin();
      authenticated == false && i != auths->end();
      ++i) {
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
      "Trying authentication type " + ssh::GetAuthenticationTypeName(*i) +
      "\r\n");

    switch(*i) {
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
  }

  delete auths;
  auths = NULL;

  // For safety, clear the sensitive data.
  key_.clear();

  if (authenticated == false) {
    mosh_->Error("ssh authentication failed: %s",
        session_->GetLastError().c_str());
    return false;
  }

  if (!DoConversation()) {
    return false;
  }

  return true;
}

bool SSHLogin::CheckFingerprint() {
  const string server_name = addr_ + ":" + port_;
  const string server_fp = session_->GetPublicKey()->MD5();

  mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
      "Fingerprint of remote ssh host (MD5):\r\n  " + server_fp + "\r\n");

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
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
        "WARNING!!! Server fingerprint differs! "
        "Possible man-in-the-middle attack.\r\n"
        "Stored fingerprint (MD5):\r\n  " + stored_fp + "\r\n");
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

vector<ssh::AuthenticationType> *SSHLogin::GetAuthTypes() {
  // Place the list of supported authentications types here, in the order
  // they should be tried.
  vector<ssh::AuthenticationType> client_auths;
  client_auths.push_back(ssh::kPublicKey);
  client_auths.push_back(ssh::kInteractive);
  client_auths.push_back(ssh::kPassword);

  mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
      "Authentication types supported by server:\r\n");
  vector<ssh::AuthenticationType> server_auths =
    session_->GetAuthenticationTypes();
  if (server_auths.size() == 0) {
    mosh_->Error("Failed to get authentication types: %s",
        session_->GetLastError().c_str());
    return NULL;
  }

  for (vector<ssh::AuthenticationType>::iterator i = server_auths.begin();
      i != server_auths.end();
      ++i) {
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
        " - " + ssh::GetAuthenticationTypeName(*i));
    if (std::find(client_auths.begin(), client_auths.end(), *i) ==
        client_auths.end()) {
      mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
          " (not supported by client)");
    }
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "\r\n");
  }

  // In order to maintain the order of client_auths, but still display
  // server_auths that are unsupported (above), we have to loop through these
  // again.
  vector<ssh::AuthenticationType> *supported_auths =
    new vector<ssh::AuthenticationType>;
  for (vector<ssh::AuthenticationType>::iterator i = client_auths.begin();
      i != client_auths.end();
      ++i) {
    if (std::find(server_auths.begin(), server_auths.end(), *i) !=
        server_auths.end()) {
      supported_auths->push_back(*i);
    }
  }

  return supported_auths;
}

bool SSHLogin::DoPasswordAuth() {
  for (int tries = RETRIES; tries > 0; --tries) {
    char input[INPUT_SIZE];
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "Password: ");
    GetKeyboardLine(input, sizeof(input), false);
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "\r\n");
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
      mosh_->Error("Password authentication failed: %s",
          session_->GetLastError().c_str());
    }
  }
  return false;
}

bool SSHLogin::DoInteractiveAuth() {
  ssh::KeyboardInteractive *kbd = session_->AuthUsingKeyboardInteractive();

  for (int tries = RETRIES; tries > 0; --tries) {
    ssh::KeyboardInteractive::Status status = kbd->GetStatus();
    while (status == ssh::KeyboardInteractive::kPending) {
      mosh_->Output(MoshClientInstance::TYPE_DISPLAY, kbd->GetName());
      mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
          kbd->GetInstruction());
      bool done = false;
      while (!done) {
        char input[INPUT_SIZE];
        mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
            kbd->GetNextPrompt());
        GetKeyboardLine(input, sizeof(input), kbd->IsAnswerEchoed());
        mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "\r\n");
        if (strlen(input) == 0) {
          // User provided no input; skip this authentication type.
          status = ssh::KeyboardInteractive::kFailed;
          tries = 0;
          break;
        }
        done = kbd->Answer(input);
        // For safety, zero the sensitive input ASAP.
        memset(input, 0, sizeof(input));
      }
      status = kbd->GetStatus();
    }
    const char *error = NULL;
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
    if (error != NULL && tries == 1) {
      // Only display error on the last try.
      mosh_->Error(error);
    }
  }
  return false;
}

bool SSHLogin::DoPublicKeyAuth() {
  for (int tries = RETRIES; tries > 0; --tries) {
    if (key_.size() == 0) {
      mosh_->Output(MoshClientInstance::TYPE_DISPLAY,
          "No ssh key found.\r\n");
      return false;
    }
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "Passphrase: ");
    char input[INPUT_SIZE];
    GetKeyboardLine(input, sizeof(input), false);
    mosh_->Output(MoshClientInstance::TYPE_DISPLAY, "\r\n");
    if (strlen(input) == 0) {
      // User provided no input; skip this authentication type.
      return false;
    }
    ssh::Key key;
    bool result = key.ImportPrivateKey(key_, input);
    memset(input, 0, sizeof(input));
    if (result == false) {
      if (tries == 1) {
        // Only display error on the last try.
        mosh_->Error("Error reading key: %s", session_->GetLastError().c_str());
      }
      continue;
    }
    if (session_->AuthUsingKey(key) == false) {
      mosh_->Error("Key auth failed: %s", session_->GetLastError().c_str());
      return false;
    }
    // If we got here, auth succeeded.
    return true;
  }
  return false;
}

bool SSHLogin::DoConversation() {
  ssh::Channel *c = session_->NewChannel();
  if (c->Execute("mosh-server new -s -c 256 -l LANG=en_US.UTF-8") == false) {
    mosh_->Error("Failed to execute mosh-server: %s",
        session_->GetLastError().c_str());
    return false;
  }
  string buf;
  if (c->Read(&buf, NULL) == false) {
    mosh_->Error("Error reading from remote ssh server: %s",
        session_->GetLastError().c_str());
    return false;
  }

  char key[23];
  char *port = new char[6];
  mosh_->set_port(port); // Takes ownership.
  size_t left_pos = 0;
  while (true) {
    const char *newline = "\r\n";
    size_t right_pos = buf.find(newline, left_pos);
    if (right_pos == string::npos) {
      mosh_->Error("Bad response when running mosh-server: '%s'", buf.c_str());
      return false;
    }
    string substr = buf.substr(left_pos, right_pos - left_pos);
    int result = sscanf(substr.c_str(), "MOSH CONNECT %5s %22s", port, key);
    if (result == 2) {
      break;
    }
    left_pos = right_pos + strlen(newline);
  }
  // TODO: This should probably be communicated more cleanly to
  // MoshClientInstance, so that it can make the determination as to what to do
  // with it.
  setenv("MOSH_KEY", key, 1);
  return true;
}
