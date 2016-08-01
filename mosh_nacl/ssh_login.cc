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

#include "mosh_nacl/ssh_login.h"

#include <string.h>  // TODO: Eliminate use of strlen().
#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mosh_nacl/make_unique.h"
#include "mosh_nacl/mosh_nacl.h"
#include "mosh_nacl/sshfp_record.h"

using std::future;
using std::move;
using std::promise;
using std::string;
using std::unique_ptr;
using std::vector;
using util::make_unique;

const int INPUT_SIZE = 256;
const int RETRIES = 3;
const string kServerCommandDefault(
    "mosh-server new -s -c 256 -l LANG=en_US.UTF-8");

namespace {

void GetKeyboardLine(char* buf, size_t len, bool echo) {
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
          const char* backspace = "\b\x1b[K";
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

}  // anonymous namespace

bool SSHLogin::AskYesNo(const string& prompt) {
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
  setenv("HOME", "dummy", 1);  // To satisfy libssh.

  if (Resolve() == false) {
    return false;
  }

  session_ =
      make_unique<ssh::Session>(resolved_addr_, atoi(port_.c_str()), user_);
  // Extend connection timeout to 30s.
  session_->SetOption(SSH_OPTIONS_TIMEOUT, 30);
  // Uncomment below for lots of debugging output.
  // session_->SetOption(SSH_OPTIONS_LOG_VERBOSITY, 30);

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

    switch (auth) {
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

bool SSHLogin::Resolve() {
  // Lookup the address.
  promise<string> addr_promise;
  promise<Resolver::Authenticity> addr_auth_promise;
  resolver_->Resolve(host_, type_, [&addr_promise, &addr_auth_promise](
                                       Resolver::Error error,
                                       Resolver::Authenticity authenticity,
                                       vector<string> results) {
    addr_auth_promise.set_value(authenticity);
    if (error == Resolver::Error::NOT_RESOLVED) {
      fprintf(stderr,
              "Could not resolve the hostname. "
              "Check the spelling and the address family.\r\n");
      addr_promise.set_value("");
      return;
    }
    if (error != Resolver::Error::OK) {
      fprintf(stderr,
              "Name resolution failed with unexpected error code: %d\r\n",
              error);
      addr_promise.set_value("");
      return;
    }
    // Only using first address.
    addr_promise.set_value(move(results[0]));
  });

  // Simultaneously lookup the SSHFP record.
  promise<vector<string>> fp_promise;
  promise<Resolver::Authenticity> fp_auth_promise;
  resolver_->Resolve(
      host_, Resolver::Type::SSHFP,
      [&fp_promise, &fp_auth_promise](Resolver::Error error,
                                      Resolver::Authenticity authenticity,
                                      vector<string> results) {
        fp_auth_promise.set_value(authenticity);
        if (error == Resolver::Error::OK) {
          fp_promise.set_value(move(results));
        } else {
          fp_promise.set_value({});
        }
        return;
      });

  // Collect the results.
  resolved_addr_ = addr_promise.get_future().get();
  resolved_fp_ = fp_promise.get_future().get();

  switch (addr_auth_promise.get_future().get()) {
    case Resolver::Authenticity::AUTHENTIC:
      printf("Authenticated DNS lookup.\r\n");
      break;
    case Resolver::Authenticity::INSECURE:
      printf("Could NOT authenticate DNS lookup.\r\n");
      break;
  }

  if (resolved_addr_.size() == 0) {
    return false;
  }

  if (!resolved_fp_.empty()) {
    switch (fp_auth_promise.get_future().get()) {
      case Resolver::Authenticity::AUTHENTIC:
        printf("Found authentic SSHFP fingerprint record(s) in DNS.\r\n");
        break;
      case Resolver::Authenticity::INSECURE:
        printf(
            "Unauthenticated SSHFP fingerprint record(s) in DNS; "
            "ignoring.\r\n");
        resolved_fp_.clear();
        break;
    }
  }
  return true;
}

bool SSHLogin::CheckFingerprint() {
  string server_name;
  if (host_.find(':') == string::npos) {
    server_name = host_ + ":" + port_;
  } else {
    server_name = "[" + host_ + "]:" + port_;
  }
  printf("Remote ssh host name/address:\r\n  %s\r\n", server_name.c_str());

  // TODO: Remove |legacy_server_name| and all the legacy fingerprint handling
  // code around it, enough time has passed that most users' fingerprints have
  // been migrated.
  string legacy_server_name;
  if (resolved_addr_.find(':') == string::npos) {
    legacy_server_name = resolved_addr_ + ":" + port_;
  } else {
    legacy_server_name = "[" + resolved_addr_ + "]:" + port_;
  }

  const ssh::Key& host_key = session_->GetPublicKey();

  // First check key against SSHFP record(s) (if any).
  if (!resolved_fp_.empty()) {
    SSHFPRecord sshfp;
    if (!sshfp.Parse(resolved_fp_)) {
      fprintf(stderr, "Authenticated SSHFP DNS record(s) are malformed!\r\n");
      // Allow to fallthrough to get the diagnostics at the end. Malformed
      // records will not validate, so this is OK.
    }
    if (!sshfp.IsValid(host_key)) {
      fprintf(
          stderr,
          "Authenticated SSHFP DNS record(s) do not validate the host key!\r\n"
          "Likely man-in-the-middle attack or misconfiguration.\r\n"
          "SSHFP records(s) are:\r\n");
      for (const auto& record : resolved_fp_) {
        fprintf(stderr, "  %s\r\n", record.c_str());
      }
      if (trust_sshfp()) {
        // If the user doesn't trust the SSHFP, a borked one shouldn't block
        // the login. All of this was for informational purposes.
        return false;
      }
    } else if (trust_sshfp()) {
      return true;
    }
  }

  // No SSHFP records. Use sync'd database of fingerprints to validate host.
  const string server_fp = host_key.MD5();
  printf("%s key fingerprint of remote ssh host (MD5):\r\n  %s\r\n",
         host_key.GetKeyType().AsString().c_str(), server_fp.c_str());
  const pp::Var stored_fp_var = known_hosts_.Get(server_name);
  if (stored_fp_var.is_undefined()) {
    // No stored fingerprint.
    // Check to see if there's a "legacy" entry (by IP address).
    const pp::Var legacy_stored_fp_var = known_hosts_.Get(legacy_server_name);
    if (!legacy_stored_fp_var.is_undefined()) {
      const string legacy_stored_fp = legacy_stored_fp_var.AsString();
      if (legacy_stored_fp == server_fp) {
        printf(
            "Fingerprints are now stored by hostname, but an old matching\r\n"
            "fingerprint for this host's IP address (%s) was found.\r\n",
            resolved_addr_.c_str());
        bool result =
            AskYesNo("Would you like to use this fingerprint for this host?");
        if (result == true) {
          known_hosts_.Set(server_name, legacy_stored_fp.c_str());
          return true;
        }
      }
    }

    bool result = AskYesNo("Server fingerprint unknown. Store and continue?");
    if (result == true) {
      known_hosts_.Set(server_name, server_fp);
      return true;
    }
  } else {
    const string stored_fp = stored_fp_var.AsString();
    if (stored_fp == server_fp) {
      return true;
    }
    printf(
        "WARNING!!! Server fingerprint differs for this host! "
        "Possible man-in-the-middle attack.\r\n"
        "Stored fingerprint (MD5):\r\n  %s\r\n",
        stored_fp.c_str());
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

// Formats a string for output. Particularly, adds '\r' after '\n'.
string FormatForOutput(const string& in) {
  string out;
  for (char c : in) {
    if (c == '\n') {
      out.append(1, '\r');
    }
    out.append(1, c);
  }
  return out;
}

bool SSHLogin::DoInteractiveAuth() {
  ssh::KeyboardInteractive& kbd = session_->AuthUsingKeyboardInteractive();

  bool displayed_instruction = false;
  for (int tries = RETRIES; tries > 0; --tries) {
    ssh::KeyboardInteractive::Status status = kbd.GetStatus();
    if (kbd.GetInstruction().size() > 0 && !displayed_instruction) {
      printf("%s\r\n", FormatForOutput(kbd.GetInstruction()).c_str());
      displayed_instruction = true;  // Don't repeat this when retrying.
    }
    while (status == ssh::KeyboardInteractive::kPending) {
      if (kbd.GetName().size() > 0) {
        printf("%s\r\n", kbd.GetName().c_str());
      }
      bool done = false;
      while (!done) {
        char input[INPUT_SIZE];
        printf("%s", FormatForOutput(kbd.GetNextPrompt()).c_str());
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
    const char* error = nullptr;
    switch (status) {
      case ssh::KeyboardInteractive::kAuthenticated:
        return true;
      case ssh::KeyboardInteractive::kPartialAuthentication:
        error = "Keyboard interactive succeeded but insufficient.";
        tries = 0;
        break;
      case ssh::KeyboardInteractive::kFailed:  // fallthrough
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
  // First try to authenticate with an SSH agent, if desired.
  if (use_agent_ && session_->AuthUsingAgent()) {
    return true;
  }

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
          fprintf(
              stderr,
              "Error reading key. This could be due to the wrong "
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
  mosh_addr_ = resolved_addr_;

  size_t left_pos = 0;
  while (true) {
    const char* newline = "\r\n";
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
