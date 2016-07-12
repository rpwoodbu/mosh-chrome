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

var kSSHDefaultPort = 22;
var kMoshDefaultPort = 60001;

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
  var prefsLink = document.querySelector('#prefs');
  prefsLink.onclick = onPrefsClick;
  var form = document.querySelector('#args');
  form.onsubmit = function() { return false; };

  migrateSettings(function() {
    loadFields();
    updateMode();
  });
};

// Mapping of old chrome.storage.local keys to their new key names.
var kLocalKeysToMigrate = {
  'field_command': 'field_server-command',
}

// Migrates settings from one form to another, to keep a tidy shop while
// avoiding losing old data. Calls callback when done (fields will not be ready
// to read until this callback).
function migrateSettings(callback) {
  var count = 0;

  for (var oldKey in kLocalKeysToMigrate) {
    chrome.storage.local.get(oldKey, function(o) {
      if (o[oldKey] !== undefined) {
        var newKey = kLocalKeysToMigrate[oldKey];
        console.log("Migrating " + oldKey + " to " + newKey);
        var newObject = {};
        newObject[newKey] = o[oldKey];
        count++;
        chrome.storage.local.set(newObject, function() {
          // Call the callback once all keys are migrated.
          count--;
          if (count == 0) {
            callback();
          }
        });
        chrome.storage.local.remove(oldKey);
      }
    });
  }

  if (count == 0) {
    // There were no migrations; call the callback here.
    callback();
  }
}

var kSyncFieldNames = [
  'addr',
  'ssh-port',
  'mosh-port',
  'family',
  'user',
  'remote-command',
  'server-command',
  'mosh-escape-key',
];

function loadFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    chrome.storage.local.get(key, function(o) {
      if (o[key] !== undefined) {
        form[field].value = o[key];
      }
    });
    // Form fields are disabled at first, so if Chrome is slow to run this
    // function, the user can't start typing ahead of the fields getting
    // loaded.
    form[field].disabled = false;
  });
}

function saveFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    if (form[field].value === "") {
      chrome.storage.local.remove(key);
    } else {
      var o = {};
      o[key] = form[field].value;
      chrome.storage.local.set(o);
    }
  });
}

function onConnectClick(e) {
  saveFields();
  var args = {}
  var form = document.querySelector('#args');
  var sshModeButton = document.querySelector('#ssh-mode');

  args['addr'] = form['addr'].value;
  if (sshModeButton.checked) {
    args['port'] = form['ssh-port'].value;
  } else {
    args['port'] = form['mosh-port'].value;
  }
  args['family'] = form['family'].value;
  args['user'] = form['user'].value;
  args['key'] = form['key'].value;
  args['remote-command'] = form['remote-command'].value;
  args['server-command'] = form['server-command'].value;
  args['mosh-escape-key'] = form['mosh-escape-key'].value;
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
  var sshPortField = document.querySelector('#ssh-port');
  var moshPortField = document.querySelector('#mosh-port');
  var usernameRow = document.querySelector('#username-row');
  var sshPortRow = document.querySelector('#ssh-port-row');
  var moshPortRow = document.querySelector('#mosh-port-row');
  var keyRow = document.querySelector('#key-row');
  var remoteCommandRow = document.querySelector('#remote-command-row');
  var serverCommandRow = document.querySelector('#server-command-row');
  var moshEscapeKeyRow = document.querySelector('#mosh-escape-key-row');

  if (sshModeButton.checked) {
    if (sshPortField.value === "") {
      sshPortField.value = kSSHDefaultPort;
    }
    usernameRow.hidden = false;
    sshPortRow.hidden = false;
    moshPortRow.hidden = true;
    keyRow.hidden = true;
    remoteCommandRow.hidden = false;
    serverCommandRow.hidden = false;
  } else {
    if (moshPortField.value === "") {
      moshPortField.value = kMoshDefaultPort;
    }
    usernameRow.hidden = true;
    sshPortRow.hidden = true;
    moshPortRow.hidden = false;
    keyRow.hidden = false;
    remoteCommandRow.hidden = true;
    serverCommandRow.hidden = true;
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

function onPrefsClick(e) {
  chrome.app.window.create(
      'mosh_prefs.html',
      {
        'bounds': {
          'width': 400,
          'height': 300,
        },
        'id': 'preferences_editor',
      });
  // Prevent default handling.
  return true;
}
