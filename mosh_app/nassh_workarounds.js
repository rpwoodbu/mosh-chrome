// nassh_workaround.js - Workarounds for nassh preferences editor.
//
// The nassh preferences editor is actually an hterm preferences editor, but not
// fully abstracted out of nassh. This file acts as a shim to make it work
// without nassh.

// Copyright 2016 Richard Woodbury
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

// We don't depend on nassh.js, so create an object for everything to hang onto.
let nassh = {};

// Provides the bare minimum functionality to construct messages actually used
// by the preferences editor.
nassh.msg = function(type, data) {
  switch (type) {
    case 'WELCOME_VERSION':
      return 'Welcome! ' + data[0] + ' ' + data[1];
    case 'JSON_PARSE_ERROR':
      return 'JSON parse error: ' + data;
    case 'PREFERENCES_RESET':
      return 'Preferences reset.';
    case 'FIELD_TERMINAL_PROFILE_PLACEHOLDER':
      // Hack to get the preferences editor to load the desired profile.
      // nassh.msg() happens to be called right after the oninput() callback is
      // setup.
      let profile = document.getElementById('profile');
      profile.oninput();
      return 'mosh';
    default:
      return 'UNEXPECTED SITUATION';
  }
};

// Stub out entirely.
nassh.exportPreferences = function() {}
