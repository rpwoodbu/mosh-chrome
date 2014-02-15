// mosh_window.js - Session window.

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

'use strict';

// Several vars will be set by the window which creates this one:
//   window.args - An object with Mosh arguments.
//   window.state - The global state object from background.js.

window.onload = function() {
  chrome.app.window.current().onClosed.addListener(onClosed);
  lib.init(execMosh, console.log.bind(console));
};

function onClosed() {
  var appWindow = chrome.app.window.current();
  delete window.state.windows[appWindow.id];
}

function execMosh() {
  var terminal = new hterm.Terminal('mosh');
  terminal.decorate(document.querySelector('#terminal'));
  terminal.onTerminalReady = function() {
    terminal.setCursorPosition(0, 0);
    terminal.setCursorVisible(true);
    terminal.runCommandClass(mosh.CommandInstance, window.args);
  };

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
};

mosh.CommandInstance.run = function(argv) {
  return new nassh.CommandInstance(argv);
};

mosh.CommandInstance.prototype.run = function() {
  // Useful for console debugging.
  window.mosh_client_ = this;

  this.io = this.argv_.io.push();
  this.io.onVTKeystroke = this.sendKeyboard_.bind(this);
  this.io.sendString = this.sendKeyboard_.bind(this);
  this.io.onTerminalResize = this.onTerminalResize_.bind(this);
  this.io.terminal_.prefs_.set('scrollbar-visible', false);

  this.moshNaCl_ = window.document.createElement('embed');
  this.moshNaCl_.style.cssText = (
      'position: absolute;' +
      'top: -99px' +
      'width: 0;' +
      'height: 0;');
  this.moshNaCl_.setAttribute('src', 'mosh_client.nmf');
  this.moshNaCl_.setAttribute('type', 'application/x-pnacl');
  this.moshNaCl_.setAttribute('key', this.argv_.argString['key']);
  this.moshNaCl_.setAttribute('addr', this.argv_.argString['addr']);
  this.moshNaCl_.setAttribute('port', this.argv_.argString['port']);
  this.moshNaCl_.setAttribute('user', this.argv_.argString['user']);
  this.moshNaCl_.setAttribute('mode', this.argv_.argString['mode']);
  if (window.ssh_key) {
    this.moshNaCl_.setAttribute('ssh_key', window.ssh_key);
    // Delete the key for good measure, although it is still available in local
    // storage.
    delete window.ssh_key;
  }

  // Delete argv_, as it contains sensitive info.
  delete this.argv_;

  this.moshNaCl_.addEventListener('load', function(e) {
    window.mosh_client_.io.print('\r\nLoaded.\r\n');
    // Remove sensitive argument attributes.
    window.mosh_client_.moshNaCl_.removeAttribute('key');
  });
  this.moshNaCl_.addEventListener('message', this.onMessage_.bind(this));
  this.moshNaCl_.addEventListener('crash', function(e) {
    window.mosh_client_.io.print('\r\nMosh NaCl crashed.\r\n');
    console.log('Mosh NaCl crashed.');
  });
  this.moshNaCl_.addEventListener('progress', this.onProgress_.bind(this));

  this.io.print("Loading NaCl module (takes a while the first time" +
      " after an update).\r\n");
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
  } else if (type == 'get_ssh_key') {
    var thiz = this;
    chrome.storage.local.get('ssh_key', function(o) {
      thiz.moshNaCl_.postMessage({'ssh_key': o['ssh_key']});
    });
  } else {
    console.log('Unknown message type: ' + JSON.stringify(e.data));
  }
};

mosh.CommandInstance.prototype.sendKeyboard_ = function(string) {
  this.moshNaCl_.postMessage({'keyboard': string});
};

mosh.CommandInstance.prototype.onTerminalResize_ = function(w, h) {
  // Send new size as an int, with the width as the high 16 bits.
  this.moshNaCl_.postMessage({'window_change': (w << 16) + h});
};

mosh.CommandInstance.prototype.onProgress_ = function(e) {
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
