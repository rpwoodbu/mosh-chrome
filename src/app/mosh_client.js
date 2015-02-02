// mosh_client.js - Session setup window.

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

function onUpdateAvailable() {
  chrome.runtime.getBackgroundPage(function(bg) {
    var updateDiv = document.querySelector('#update');
    if (bg.state.updateAvailable != null) {
      var html = '';
      html += '<strong>Update to v' + bg.state.updateAvailable +
          ' is available.</strong><br>';
      html += 'Close all Mosh windows to update. See changelog for details.';
      updateDiv.innerHTML = html;
    } else {
      updateDiv.innerHTML = '';
    }
  });
}

window.onload = function() {
  var versionDiv = document.querySelector('#version');
  var manifest = chrome.runtime.getManifest();
  versionDiv.innerText = 'v' + manifest['version'];

  // "Fire" this event in case it came in while this window wasn't open (very
  // likely). The background page is responsible for propagating this.
  onUpdateAvailable();

  var connectButton = document.querySelector('#connect');
  connectButton.onclick = onConnectClick;
  var sshModeButton = document.querySelector('#ssh-mode');
  sshModeButton.onchange = updateMode;
  var manualModeButton = document.querySelector('#manual-mode');
  manualModeButton.onchange = updateMode;
  var sshKeyLink = document.querySelector('#ssh-key');
  sshKeyLink.onclick = onSshKeyClick;
  loadFields();
  var form = document.querySelector('#args');
  form.onsubmit = function() { return false; };
  updateMode();
};

var kSyncFieldNames = [ 'addr', 'port', 'user', 'command' ];

function loadFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    chrome.storage.local.get(key, function(o) {
      if (o[key] !== undefined) {
        form[field].value = o[key];
      }
    });
  });
}

function saveFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    var o = {};
    o[key] = form[field].value;
    chrome.storage.local.set(o);
  });
}

function onConnectClick(e) {
  saveFields();
  var args = {}
  var form = document.querySelector('#args');
  args['addr'] = form['addr'].value;
  args['port'] = form['port'].value;
  args['user'] = form['user'].value;
  args['key'] = form['key'].value;
  args['command'] = form['command'].value;
  for (var i = 0; i < form['mode'].length; ++i) {
    if (form['mode'][i].checked) {
      args['mode'] = form['mode'][i].value;
      break;
    }
  }

  // Define an ID that should, usually, uniquely define a connection to a
  // server. This will preserve the window position across sessions. But still
  // allow multiple simultaneous connections to the same server.
  var id = 'mosh_window_' +
    args['mode'] + '_' +
    args['user'] + '@' +
    args['addr'] + ':' +
    args['port'];

  chrome.runtime.getBackgroundPage(function(bg) {
    while (id in bg.state.windows) {
      // ID already exists. Keep adding underscores until it doesn't.
      id += '_';
    }

    chrome.app.window.create(
        'mosh_window.html',
        {
          'id': id,
        },
        function(createdWindow) {
          bg.state.windows[id] = createdWindow;
          createdWindow.contentWindow.args = args;
          createdWindow.contentWindow.state = bg.state;
          chrome.app.window.current().close();
        });
  });
};

function updateMode(e) {
  var sshModeButton = document.querySelector('#ssh-mode');
  var portField = document.querySelector('#port');
  var usernameRow = document.querySelector('#username-row');
  var keyRow = document.querySelector('#key-row');
  var commandRow = document.querySelector('#command-row');

  if (sshModeButton.checked) {
    portField.value = 22;
    usernameRow.hidden = false;
    keyRow.hidden = true;
    commandRow.hidden = false;
  } else {
    portField.value = 60001;
    usernameRow.hidden = true;
    keyRow.hidden = false;
    commandRow.hidden = true;
  }
}

function onSshKeyClick(e) {
  chrome.app.window.create(
      'ssh_key.html',
      {
        'bounds': {
          'width': 400,
          'height': 300,
        },
        'id': 'ssh_key',
      });
  // Prevent default handling.
  return true;
}
