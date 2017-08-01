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

  var moshVersionDiv = document.querySelector('#mosh_version');
  moshVersionDiv.innerText = '(' + moshVersion + ')';

  // "Fire" this event in case it came in while this window wasn't open (very
  // likely). The background page is responsible for propagating this.
  onUpdateAvailable();

  var connectButton = document.querySelector('#connect');
  connectButton.onclick = onConnectClick;
  var sshModeButton = document.querySelector('#ssh-mode');
  sshModeButton.onchange = updateMode;
  var manualModeButton = document.querySelector('#manual-mode');
  manualModeButton.onchange = updateMode;
  var gpdnsCheckbox = document.querySelector('#google-public-dns');
  gpdnsCheckbox.onchange = onGpdnsCheckboxChange;
  var sshKeyLink = document.querySelector('#ssh-key');
  sshKeyLink.onclick = onSshKeyClick;
  var prefsLink = document.querySelector('#prefs');
  prefsLink.onclick = onPrefsClick;
  var form = document.querySelector('#args');
  form.onsubmit = function() {
    return false;
  };

  // Add drop-down menu for MOSH_ESCAPE_KEY, listing all ASCII characters from
  // 0x01 to 0x7F, as well an initial entry "default" to not pass
  // MOSH_ESCAPE_KEY at all.
  form['mosh-escape-key'].add(new Option('default', '', true));
  for (var c = 0x01; c <= 0x7F; ++c) {
    // For c < 0x20 and c == 0x7F, see
    // https://en.wikipedia.org/wiki/C0_and_C1_control_codes#C0_.28ASCII_and_derivatives.29
    var keyName;
    if (c < 0x20) {
      keyName = 'Ctrl+' + String.fromCharCode(0x40 + c);
    } else if (c == 0x20) {
      keyName = 'SPACE';
    } else if (c < 0x7F) {
      keyName = String.fromCharCode(c);
    } else {
      keyName = 'Ctrl+?';
    }
    form['mosh-escape-key'].add(new Option(
        keyName,
        // Value element of <option> element must be HTML-encoded.
        '&#x' + c.toString(16), false));
  }

  migrateSettings(function() {
    loadFields();
    updateMode();
  });

  // Set window to precise dimensions to fit everything perfectly.
  var margin = parseInt(
      window.getComputedStyle(document.body).getPropertyValue('margin'));
  var mainTable = document.querySelector('#main-table');
  var height = mainTable.scrollHeight + (margin * 2);
  var width = mainTable.scrollWidth + (margin * 2);
  var bounds = chrome.app.window.current().innerBounds;
  bounds.setSize(width, height);
  bounds.setMinimumSize(width, height);
};

// Mapping of old chrome.storage.local keys to their new key names.
var kLocalKeysToMigrate =
    {
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
        console.log('Migrating ' + oldKey + ' to ' + newKey);
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
  'google-public-dns',
  'trust-sshfp',
];

function loadFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    chrome.storage.local.get(key, function(o) {
      if (o[key] !== undefined) {
        if (form[field].type === 'checkbox') {
          form[field].checked = o[key] === 'true' ? true : false;
        } else {
          form[field].value = o[key];
        }
      }
      // Call onchange callback if it exists.
      var onchange = form[field].onchange;
      if (onchange != undefined) {
        onchange();
      }
    });
  });
}

function saveFields() {
  var form = document.querySelector('#args');
  kSyncFieldNames.forEach(function(field) {
    var key = 'field_' + field;
    if (form[field].value === '') {
      chrome.storage.local.remove(key);
    } else {
      var o = {};
      if (form[field].type === 'checkbox') {
        o[key] = form[field].checked ? 'true' : 'false';
      } else {
        o[key] = form[field].value;
      }
      chrome.storage.local.set(o);
    }
  });
}

function decodeHtml(html) {
  var txt = document.createElement('textarea');
  txt.innerHTML = html;
  return txt.value;
}

function onConnectClick(e) {
  saveFields();
  var args = {};
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
  var decodedMoshEscapeKey = decodeHtml(form['mosh-escape-key'].value);
  if (decodedMoshEscapeKey !== '') {
    args['mosh-escape-key'] = decodedMoshEscapeKey;
  }
  if (form['google-public-dns'].checked) {
    args['dns-resolver'] = 'google-public-dns';
  }
  args['trust-sshfp'] = form['trust-sshfp'].checked ? 'true' : 'false';
  for (var i = 0; i < form['mode'].length; ++i) {
    if (form['mode'][i].checked) {
      args['mode'] = form['mode'][i].value;
      break;
    }
  }

  // Define an ID that should, usually, uniquely define a connection to a
  // server. This will preserve the window position across sessions. But still
  // allow multiple simultaneous connections to the same server.
  var id = 'mosh_window_' + args['mode'] + '_' + args['user'] + '@' +
      args['addr'] + ':' + args['port'];

  chrome.runtime.getBackgroundPage(function(bg) {
    while (id in bg.state.windows) {
      // ID already exists. Keep adding underscores until it doesn't.
      id += '_';
    }

    chrome.app.window.create(
        'mosh_window.html', {
          'id': id,
        },
        function(createdWindow) {
          bg.state.windows[id] = createdWindow;
          createdWindow.contentWindow.args = args;
          createdWindow.contentWindow.state = bg.state;
          // Adding a listener to onClosed is tricky, as a lot of functionality
          // is missing while the window is being destroyed. Using setTimeout()
          // on the background window ensures the callback is run in its
          // (persistent) context.
          bg.setTimeout(function() {
            createdWindow.onClosed.addListener(function() {
              bg.onSessionWindowClosed(id);
            }, 0)
          });
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
    if (sshPortField.value === '') {
      sshPortField.value = kSSHDefaultPort;
    }
    usernameRow.hidden = false;
    sshPortRow.hidden = false;
    moshPortRow.hidden = true;
    keyRow.hidden = true;
    remoteCommandRow.hidden = false;
    serverCommandRow.hidden = false;
  } else {
    if (moshPortField.value === '') {
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
  chrome.app.window.create('ssh_key.html', {
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
  chrome.app.window.create('mosh_prefs.html', {
    'bounds': {
      'width': 400,
      'height': 300,
    },
    'id': 'preferences_editor',
  });
  // Prevent default handling.
  return true;
}

function onGpdnsCheckboxChange(e) {
  var gpdnsCheckbox = document.querySelector('#google-public-dns');
  var sshfpCheckbox = document.querySelector('#trust-sshfp');
  sshfpCheckbox.disabled = !gpdnsCheckbox.checked;
}
