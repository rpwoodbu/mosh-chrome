// background.js - The background "page" that is always running.

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

// This stores the state of the app. It may be used by all child windows.
window.state = {};
// This stores the version available for update, or null if none.
window.state.updateAvailable = null;
// This is a map of all open session windows.
window.state.windows = {};

function newSession() {
  chrome.app.window.create(
    'mosh_client.html',
    {
      'bounds': {
        'width': 450,
        'height': 180
      },
      'id': 'mosh_client',
    });
};

chrome.app.runtime.onLaunched.addListener(newSession);

function updateAvailable(e) {
  window.state.updateAvailable = e.version;
  var w = chrome.app.window.get('mosh_client');
  if (w != null) {
    w.contentWindow.onUpdateAvailable();
  }

  var message = 'Update to v' + e.version + ' is available.';
  message += ' Close all Mosh windows to update. See changelog for details.';
  chrome.notifications.create(
      'update_notification',
      {
        'type': 'basic',
        'title': 'Mosh update available',
        'message': message,
        'iconUrl': 'laptop_terminal.png',
        'priority': -2,
      },
      function(id) {});
};

chrome.runtime.onUpdateAvailable.addListener(updateAvailable);

chrome.contextMenus.create({
  'type': 'normal',
  'id': 'new_session',
  'title': 'New session',
  'contexts': ['launcher'],
});

chrome.contextMenus.onClicked.addListener(function() { newSession(); });
