// mosh_window.js - Session window.

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

'use strict';

// Several vars will be set by the window which creates this one:
//   window.args - An object with Mosh arguments.
//   window.state - The global state object from background.js.

window.onload = function() {
  lib.init(execMosh, console.log.bind(console));
};

function execMosh() {
  var terminal = new hterm.Terminal('mosh');
  terminal.decorate(document.querySelector('#terminal'));
  terminal.onTerminalReady = function() {
    terminal.setCursorPosition(0, 0);
    terminal.setCursorVisible(true);
    terminal.runCommandClass(mosh.CommandInstance, window.args);
  };

  // Don't exit fullscreen with ESC.
  terminal.document_.onkeyup = function(e) {
    if (e.keyCode == 27) {
      e.preventDefault();
    }
  };

  // Workaround to return focus to terminal on fullscreen.
  // See https://code.google.com/p/chromium/issues/detail?id=402340
  var appWindow = chrome.app.window.current();
  appWindow.onFullscreened.addListener(function() {
    appWindow.hide();
    appWindow.show();
    terminal.focus();
  });

  document.title += ' - ' + window.args['addr'];

  // Useful for console debugging.
  window.term_ = terminal;
};

var mosh = {};

mosh.CommandInstance = function(argv) {
  // Command arguments.
  this.argv_ = argv;

  // Command environment.
  this.environment_ = argv.environment || {};

  // hterm.Terminal.IO instance.
  this.io = null;

  // Whether the NaCl module is running.
  this.running_ = false;

  // Port to an SSH agent.
  this.agentPort_ = null;

  // App ID of an SSH agent.
  // TODO: Make this a user setting.
  this.agentAppID_ = 'beknehfpfkghjoafdifaflglpjkojoco';
};

mosh.CommandInstance.prototype.run = function() {
  // Useful for console debugging.
  window.mosh_client_ = this;

  this.io = this.argv_.io.push();
  this.io.onVTKeystroke = this.sendKeyboard_.bind(this);
  this.io.sendString = this.sendKeyboard_.bind(this);
  this.io.onTerminalResize = this.onTerminalResize_.bind(this);

  // Mosh has no scrollback buffer (currently).
  this.io.terminal_.prefs_.set('scrollbar-visible', false);

  this.moshNaCl_ = window.document.createElement('embed');
  this.moshNaCl_.style.cssText =
      ('position: absolute;' +
       'top: -99px' +
       'width: 0;' +
       'height: 0;');
  this.moshNaCl_.setAttribute('src', nacl_nmf_file);
  this.moshNaCl_.setAttribute('type', nacl_mime_type);
  for (var k in this.argv_.argString) {
    this.moshNaCl_.setAttribute(k, this.argv_.argString[k]);
  }

  // Delete argv_, as it contains sensitive info.
  delete this.argv_;

  // Output special text (e.g., ANSI escape sequences) if desired.
  chrome.storage.sync.get('term_init_string', function(o) {
    if (o != null) {
      window.mosh_client_.io.print(o['term_init_string']);
    }
  });

  this.moshNaCl_.addEventListener('load', this.onLoad_.bind(this));
  this.moshNaCl_.addEventListener('message', this.onMessage_.bind(this));
  this.moshNaCl_.addEventListener('crash', this.onCrash_.bind(this));
  this.moshNaCl_.addEventListener('progress', this.onProgress_.bind(this));

  this.discoverAgentThenInsertMosh();
};

mosh.CommandInstance.prototype.discoverAgentThenInsertMosh = function() {
  this.agentPort_ = chrome.runtime.connect(this.agentAppID_);
  if (this.agentPort_ === null) {
    this.moshNaCl_.setAttribute('use-agent', false);
    this.insertMosh();
    return;
  }

  var probeOnMessage = function(message) {
    // We got a response; there is an agent.
    this.agentPort_.onMessage.removeListener(probeOnMessage);
    this.agentPort_.onDisconnect.removeListener(onDisconnect);

    this.agentPort_.onMessage.addListener(function(message) {
      if (message['type'] !== 'auth-agent@openssh.com') {
        console.error('Got unexpected message from SSH agent:');
        console.error(message);
        return;
      }
      this.moshNaCl_.postMessage({'ssh_agent': message['data']});
    }.bind(this));

    this.moshNaCl_.setAttribute('use-agent', true);
    this.insertMosh();
  }.bind(this);

  this.agentPort_.onMessage.addListener(probeOnMessage);

  var onDisconnect = function() {
    this.agentPort_.onMessage.removeListener(probeOnMessage);
    this.agentPort_.onDisconnect.removeListener(onDisconnect);
    this.moshNaCl_.setAttribute('use-agent', false);
    this.insertMosh();
  }.bind(this);

  this.agentPort_.onDisconnect.addListener(onDisconnect);

  // To probe for an agent, post an empty message just to get a response or a
  // disconnection.
  this.agentPort_.postMessage({'type': 'auth-agent@openssh.com', 'data': [0]});
};

mosh.CommandInstance.prototype.insertMosh = function() {
  if (nacl_mime_type == 'application/x-pnacl') {
    this.io.print(
        'Loading NaCl module (takes a while the first time' +
        ' after an update).\r\n');
  }
  document.body.insertBefore(this.moshNaCl_, document.body.firstChild);
};

mosh.CommandInstance.prototype.onMessage_ = function(e) {
  var data = e.data['data'];
  var type = e.data['type'];
  if (type == 'display') {
    this.io.print(data);
  } else if (type == 'log') {
    console.log(String(data));
  } else if (type == 'error') {
    // TODO: Find a way to output errors that doesn't interfere with the
    // terminal window.
    var output = String(data);
    if (output.search('\r\n') == -1) {
      output = output.replace('\n', '\r\n');
    }
    this.io.print(output + '\r\n');
    console.error(output);
  } else if (type.match(/^get_.+/)) {
    var thiz = this;
    var name = type.slice(4);
    chrome.storage.local.get(name, function(o) {
      var result = {};
      result[name] = o[name];
      thiz.moshNaCl_.postMessage(result);
    });
  } else if (type.match(/^set_.+/)) {
    var name = type.slice(4);
    var param = {};
    param[name] = data;
    // Wash out any string "objects" by going through JSON (hacky).
    var j = JSON.stringify(param);
    param = JSON.parse(j);
    chrome.storage.local.set(param);
  } else if (type.match(/^sync_get_.+/)) {
    var thiz = this;
    var name = type.slice(9);
    chrome.storage.sync.get(name, function(o) {
      var result = {};
      result[name] = o[name];
      thiz.moshNaCl_.postMessage(result);
    });
  } else if (type.match(/^sync_set_.+/)) {
    var name = type.slice(9);
    var param = {};
    param[name] = data;
    // Wash out any string "objects" by going through JSON (hacky).
    var j = JSON.stringify(param);
    param = JSON.parse(j);
    chrome.storage.sync.set(param);
  } else if (type == 'ssh-agent') {
    this.sendToAgent_(data);
  } else if (type == 'exit') {
    this.exit_('Mosh has exited.');
  } else {
    console.log('Unknown message type: ' + JSON.stringify(e.data));
  }
};

mosh.CommandInstance.prototype.sendKeyboard_ = function(string) {
  if (this.running_) {
    // Convert this to an array of codepoints to avoid any Unicode shenanigans,
    // which can interfere with terminal escape sequences.
    var codePoints = [];
    for (var i = 0; i < string.length; i++) {
      codePoints.push(string.codePointAt(i));
    }
    this.moshNaCl_.postMessage({'keyboard': codePoints});
  } else if (string == 'x') {
    window.close();
  }
};

mosh.CommandInstance.prototype.onTerminalResize_ = function(w, h) {
  // Send new size as an int, with the width as the high 16 bits.
  this.moshNaCl_.postMessage({'window_change': (w << 16) + h});
};

mosh.CommandInstance.prototype.onProgress_ = function(e) {
  if (nacl_mime_type != 'application/x-pnacl') {
    // Don't bother with this unless it is a portable NaCl executable. Native
    // executables load essentially instantly.
    return;
  }
  if (e.lengthComputable && e.total > 0) {
    var divisions = 15;
    var fraction = event.loaded / event.total;
    var numDots = Math.floor(fraction * divisions);
    var numSpaces = divisions - numDots;
    var output = '\r[';
    for (var i = 0; i < numDots; i++) {
      output += '.';
    }
    for (var i = 0; i < numSpaces; i++) {
      output += ' ';
    }
    output += ']';
    this.io.print(output);
  }
};

mosh.CommandInstance.prototype.onLoad_ = function(e) {
  if (nacl_mime_type == 'application/x-pnacl') {
    this.io.print('\r\nLoaded.\r\n');
  }
  this.running_ = true;
  // Remove sensitive argument attributes.
  this.moshNaCl_.removeAttribute('key');
};

mosh.CommandInstance.prototype.onCrash_ =
    function(e) {
  this.exit_('Mosh NaCl crashed.');
}

    mosh.CommandInstance.prototype.exit_ = function(output) {
  this.io.print('\r\n' + output + '\r\n');
  console.log(output);
  this.io.print('Press "x" to close the window.\r\n');
  this.running_ = false;
};

// Send data to an SSH agent.
mosh.CommandInstance.prototype.sendToAgent_ = function(data) {
  var message = {'type': 'auth-agent@openssh.com', 'data': data};
  this.agentPort_.postMessage(message);
}
