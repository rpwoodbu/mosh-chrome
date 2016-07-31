// mosh_client.js - Session setup window.

// Copyright 2014, 2015 Richard Woodbury
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

var kSSHKey = 'ssh_key';

window.onload = function() {
  chrome.storage.local.get(kSSHKey, function(keys) {
    if (keys[kSSHKey] !== undefined) {
      var field = document.querySelector('#key');
      field.placeholder = 'Key is saved, but hidden for security. ' +
          'Enter another key to replace the existing key, ' +
          'or leave this field blank and hit save to erase.';
    }
  });
  var saveButton = document.querySelector('#save');
  saveButton.onclick = onSaveClick;
  var form = document.querySelector('#key-form');
  form.onsubmit = function() { return false; };
};

function onSaveClick(e) {
  var field = document.querySelector('#key');
  if (field.value === '') {
    chrome.storage.local.remove(kSSHKey);
  } else {
    var o = {};
    o[kSSHKey] = field.value;
    chrome.storage.local.set(o);
  }
  window.close();
}
