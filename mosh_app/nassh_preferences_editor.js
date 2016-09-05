// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ===========================================================================
// Note: This is a forked version of nassh_preferences_editor.js:
// https://chromium.googlesource.com/apps/libapps/+/master/nassh/js/nassh_preferences_editor.js
//
// The LICENSE file mentioned above covers the entirety of this file in its
// current state, and is available here:
// http://src.chromium.org/viewvc/chrome/trunk/src/LICENSE?view=markup
// ===========================================================================

'use strict';

lib.rtdep('lib.colors', 'hterm.PreferenceManager');

var nassh = {};  // we don't depend on nassh.js

// CSP means that we can't kick off the initialization from the html file,
// so we do it like this instead.
window.onload = function() {
  function setupPreferences() {
    var manifest = chrome.runtime.getManifest();

    // Create a local hterm instance so people can see their changes live.
    var term = new hterm.Terminal();
    term.onTerminalReady = function() {
      var io = term.io.push();
      io.onVTKeystroke = io.print;
      io.println('# Welcome! ' + manifest.name + ' ' + manifest.version);
      io.print('$ ./configure && make && make install');
    };
    term.decorate(document.querySelector('#terminal'));
    term.installKeyboard();

    // Useful for console debugging.
    window.term_ = term;

    var prefsEditor = new nassh.PreferencesEditor('mosh');

    // Set up icon on the left side.
    if (document.getElementById('icon') != null) {
      document.getElementById('icon').src = '../' + manifest.icons['128'];
    }

    // Set up reset button.
    document.getElementById('reset').onclick = function() {
      prefsEditor.resetAll();
    };

    // Allow people to reset individual fields by pressing escape.
    document.onkeyup = function(e) {
      if (document.activeElement.name == 'settings' && e.keyCode == 27)
        prefsEditor.reset(document.activeElement);
    };
  }

  lib.init(setupPreferences);
};

/**
 * Class for editing hterm profiles.
 *
 * @param {string} opt_profileId Optional profile name to read settings from;
 *     defaults to the "default" profile.
 */
nassh.PreferencesEditor = function(opt_profileId) {
  this.selectProfile(opt_profileId || 'default');
};

/**
 * Debounce action on input element.
 *
 * This way people can type up a setting before seeing an update.
 * Useful with settings such as font names or sizes.
 *
 * @param {object} input An HTML input element to pass down to callback.
 * @param {function} callback Function to call after debouncing while passing
 *     it the input object.
 * @param {integer} opt_timeout Optional how long to debounce.
 */
nassh.PreferencesEditor.debounce = function(input, callback, opt_timeout) {
  clearTimeout(input.timeout);
  input.timeout = setTimeout(function() {
    callback(input);
    input.timeout = null;
  }, opt_timeout || 500);
};

/**
 * Select a profile for editing.
 *
 * This will load the settings and update the HTML display.
 *
 * @param {string} profileId The profile name to read settings from.
 */
nassh.PreferencesEditor.prototype.selectProfile = function(profileId) {
  term_.setProfile(profileId);
  var prefsEditor = this;
  var prefs = new hterm.PreferenceManager(profileId);
  this.prefs_ = prefs;
  prefs.readStorage(function() {
    prefs.notifyAll();
    prefsEditor.syncPage();
  });
};

/**
 * Save the HTML color state to the preferences.
 *
 * Since the HTML5 color picker does not support alpha, we have to split
 * the rgb and alpha information across two input objects.
 *
 * @param {string} key The HTML input.id to use to locate the color input
 *     object.  By appending ':alpha' to the key name, we can also locate
 *     the range input object.
 */
nassh.PreferencesEditor.prototype.colorSave = function(key) {
  var cinput = document.getElementById(key);
  var ainput = document.getElementById(key + ':alpha');
  var rgb = lib.colors.hexToRGB(cinput.value);
  this.prefs_.set(key, lib.colors.setAlpha(rgb, ainput.value / 100));
};

/**
 * Save the HTML state to the preferences.
 *
 * @param {object} input An HTML input element to update the corresponding
 *     preferences key.  Uses input.id to locate relevant preference.
 */
nassh.PreferencesEditor.prototype.save = function(input) {
  // Skip ones we don't yet handle.
  if (input.disabled) return;

  var keys = input.id.split(':');
  var key = keys[0];
  var prefs = this.prefs_;
  switch (input.type) {
    case 'checkbox':
      if (input.indeterminate) {
        prefs.set(key, null);
      } else {
        prefs.set(key, input.checked);
      }
      break;
    case 'number':
      prefs.set(key, Number(input.value));
      break;
    case 'range':
    case 'color':
      this.colorSave(key);
      break;
    case 'text':
    case 'textarea':
      var value = input.value;
      if (input.data == 'JSON') {
        try {
          value = JSON.parse(value);
        } catch (err) {
          this.notify('JSON parse error: ' + key + ': ' + err, 5000);
          value = prefs.get(key);
        }
      }
      prefs.set(key, value);
      break;
  }
};

/**
 * Sync the preferences state to the HTML color objects.
 *
 * @param {string} key The HTML input.id to use to locate the color input
 *     object.  By appending ':alpha' to the key name, we can also locate
 *     the range input object.
 * @param {object} pref The preference object to get the current state from.
 * @return {string} The rgba color information.
 */
nassh.PreferencesEditor.prototype.colorSync = function(key, pref) {
  var cinput = document.getElementById(key);
  var ainput = document.getElementById(key + ':alpha');

  var rgba = lib.colors.normalizeCSS(pref);

  cinput.value = lib.colors.rgbToHex(rgba);
  if (rgba) {
    ainput.value = lib.colors.crackRGB(rgba)[3] * 100;
  } else {
    ainput.value = ainput.max;
  }

  return rgba;
};

/**
 * Sync the preferences state to the HTML object.
 *
 * @param {Object} input An HTML input element to update the corresponding
 *     preferences key.  Uses input.id to locate relevant preference.
 */
nassh.PreferencesEditor.prototype.sync = function(input) {
  var keys = input.id.split(':');
  var key = keys[0];
  var pref = this.prefs_.get(key);

  if (input.type == 'color' || input.type == 'range') {
    var rgba = this.colorSync(key, pref);
  } else if (input.data == 'JSON') {
    input.value = JSON.stringify(pref);
  } else {
    input.value = pref;
  }
  switch (typeof pref) {
    case 'boolean':
      if ('data' in input) {
        // Handle tristate options.
        input.indeterminate = false;
        input.data = pref ? 2 : 0;
      }
      input.checked = pref;
      break;
  }
};

/**
 * Update preferences from HTML input objects when the input changes.
 *
 * This is a helper that should be used in an event handler (e.g. onchange).
 * Should work with any input type.
 *
 * @param {Object} input An HTML input element to update from.
 */
nassh.PreferencesEditor.prototype.onInputChange = function(input) {
  this.save(input);
  this.sync(input);
};

/**
 * Update preferences from HTML checkbox input objects when the input changes.
 *
 * This is a helper that should be used in an event handler (e.g. onchange).
 * Used with checkboxes for tristate values (true/false/null).
 *
 * @param {Object} input An HTML checkbox input element to update from.
 */
nassh.PreferencesEditor.prototype.onInputChangeTristate = function(input) {
  switch (input.data % 3) {
    case 0:  // unchecked -> indeterminate
      input.indeterminate = true;
      break;
    case 1:  // indeterminate -> checked
      input.checked = true;
      break;
    case 2:  // checked -> unchecked
      input.checked = false;
      break;
  }
  ++input.data;
  this.onInputChange(input);
};

/**
 * Update the preferences page to reflect current preference object.
 *
 * Will basically rewrite the displayed HTML code on the fly.
 */
nassh.PreferencesEditor.prototype.syncPage = function() {
  var prefsEditor = this;

  var eles = document.getElementById('settings');

  // Clear out existing settings table.
  while (eles.hasChildNodes()) {
    eles.removeChild(eles.firstChild);
  }

  // Create the table of settings.
  var typeMap = {
    'boolean': 'checkbox',
    'number': 'number',
    'object': 'text',
    'string': 'text',
  };
  for (var key in this.prefs_.prefRecords_) {
    var input = document.createElement('input');
    var pref = this.prefs_.get(key);

    var onchangeCursorReset = function() {
      nassh.PreferencesEditor.debounce(this, function(input) {
        // Chrome has a bug where it resets cursor position on us when
        // we debounce the input.  So manually save & restore cursor.
        var i = input.selectionStart;
        prefsEditor.onInputChange(input);
        if (document.activeElement === input) input.setSelectionRange(i, i);
      });
    };
    var onchange = function() {
      nassh.PreferencesEditor.debounce(
          this, function(input) { prefsEditor.onInputChange(input); });
    };
    var oninput = null;

    var keyParts = key.split('-');
    if (key == 'enable-bold' || key == 'mouse-paste-button') {
      input.indeterminate = true;
      input.type = 'checkbox';
      input.data = 1;
      onchange = function() { prefsEditor.onInputChangeTristate(this); };
    } else if (keyParts[keyParts.length - 1] == 'color') {
      input.type = 'color';
    } else {
      var type = typeof pref;
      switch (type) {
        case 'object':
          // We'll use JSON to go between object/user text.
          input = document.createElement('textarea');
          input.data = 'JSON';
          onchange = onchangeCursorReset;
          break;
        case 'string':
          // Save simple strings immediately.
          oninput = onchangeCursorReset;
          onchange = null;
          break;
      }
      if (type != 'object') {
        input.type = typeMap[type];
      }
    }

    input.name = 'settings';
    input.id = key;
    input.onchange = onchange;
    input.oninput = oninput;

    // We want this element structure when we're done:
    // <div class='text'>
    //  <label>
    //    <span class='profile-ui'>
    //      <input ...>
    //    </span>
    //    <span>this-preference-setting-name</span>
    //  </label>
    // </div>
    var div = document.createElement('div');
    var label = document.createElement('label');
    var span_input = document.createElement('span');
    var span_text = document.createElement('span');

    div.className = input.type;
    span_input.className = 'profile-ui';
    span_text.innerText = key;

    div.appendChild(label);
    span_input.appendChild(input);
    label.appendChild(span_input);
    label.appendChild(span_text);
    eles.appendChild(div);

    if (input.type == 'color') {
      // Since the HTML5 color picker does not support alpha,
      // we have to create a dedicated slider for it.
      var ainput = document.createElement('input');
      ainput.type = 'range';
      ainput.id = key + ':alpha';
      ainput.min = '0';
      ainput.max = '100';
      ainput.name = 'settings';
      ainput.onchange = onchange;
      ainput.oninput = oninput;
      span_input.appendChild(ainput);
    }

    this.sync(input);
  }
};

/**
 * Reset all preferences to their default state and update the HTML objects.
 */
nassh.PreferencesEditor.prototype.resetAll = function() {
  var settings = document.getElementsByName('settings');

  this.prefs_.resetAll();
  for (var i = 0; i < settings.length; ++i) {
    this.sync(settings[i]);
  }
  this.notify('Preferences reset');
};

/**
 * Reset specified preference to its default state.
 *
 * @param {object} input An HTML input element to reset.
 */
nassh.PreferencesEditor.prototype.reset = function(input) {
  var keys = input.id.split(':');
  var key = keys[0];
  this.prefs_.reset(key);
  this.sync(input);
};

/**
 * Display a message to the user.
 *
 * @param {string} msg The string to show to the user.
 * @param {integer} opt_timeout Optional how long to show the message.
 */
nassh.PreferencesEditor.prototype.notify = function(msg, opt_timeout) {
  // Update status to let user know options were updated.
  clearTimeout(this.notifyTimeout_);
  var status = document.getElementById('label_status');
  status.innerText = msg;
  this.notifyTimeout_ = setTimeout(function() {
    status.innerHTML = '&nbsp;';
  }, opt_timeout || 1000);
};
