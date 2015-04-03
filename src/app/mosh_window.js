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

  // Don't exit fullscreen with ESC
  terminal.document_.onkeyup = function(e) {
    if (e.keyCode == 27) e.preventDefault()
  };

  // Workaround to return focus to terminal on fullscreen
  // See https://code.google.com/p/chromium/issues/detail?id=402340
  var appWindow = chrome.app.window.current();
  appWindow.onFullscreened.addListener(function () {
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

  // Mosh has no scrollback butter (currently).
  this.io.terminal_.prefs_.set('scrollbar-visible', false);
  // Makes Unicode input work.
  this.io.terminal_.prefs_.set('send-encoding', 'raw');

  this.moshNaCl_ = window.document.createElement('embed');
  this.moshNaCl_.style.cssText = (
      'position: absolute;' +
      'top: -99px' +
      'width: 0;' +
      'height: 0;');
  this.moshNaCl_.setAttribute('src', 'mosh_client.nmf');
  this.moshNaCl_.setAttribute('type', 'application/x-pnacl');
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
  } else if (type == 'exit') {
    this.exit_('Mosh has exited.');
  } else {
    console.log('Unknown message type: ' + JSON.stringify(e.data));
  }
};

mosh.CommandInstance.prototype.sendKeyboard_ = function(string) {
  if (this.running_) {
    this.moshNaCl_.postMessage({'keyboard': string});
  } else if (string == 'x') {
    window.close();
  }
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

mosh.CommandInstance.prototype.onLoad_ = function(e) {
  this.io.print('\r\nLoaded.\r\n');
  this.running_ = true;
  // Remove sensitive argument attributes.
  this.moshNaCl_.removeAttribute('key');
};

mosh.CommandInstance.prototype.onCrash_ = function(e) {
  this.exit_('Mosh NaCl crashed.');
}

mosh.CommandInstance.prototype.exit_ = function(output) {
  this.io.print('\r\n' + output + '\r\n');
  console.log(output);
  this.io.print('Press "x" to close the window.\r\n');
  this.running_ = false;
};
