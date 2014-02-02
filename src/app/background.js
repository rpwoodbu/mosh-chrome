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
// This is a map of all open session windows.
window.state.windows = {};

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create(
    'mosh_client.html',
    {
      'id': 'mosh_client',
    });
});
