Mosh for Chrome
===============

Main author: Richard Woodbury (rpwoodbu@mybox.org)

This is a [Mosh](http://mosh.mit.edu) client port for the
[Chrome](http://www.google.com/chrome/) web browser using
[Native Client](https://developers.google.com/native-client). It is
particularly useful with [Chrome OS](http://www.google.com/chromeos).

Building
--------

  You can build the dev track simply by running:

    $ ./build.sh dev

  The first time this is run, this will download and build most of the
  dependencies, including Bazel, the build system. This can take a while, so be
  patient. Subsequent builds will be extremely fast; Bazel excels at doing
  incremental builds.

  Don't be alarmed if you see a few warnings. In particular, the linker may
  complain about duplicated symbols.

  If successful, Bazel will output something like this:

    Target //:mosh_chrome up-to-date:
      bazel-genfiles/mosh_chrome_dev.zip

  That .zip file contains the entire app. To load it into Chrome, unzip it into
  a directory on your machine, then go to chrome://extensions, enable
  "Developer mode", and click the "Load unpacked extension button", directing
  it to said directory. Then the app will be launchable from the app screen and
  the extensions screen.

  To distribute the app, build the release track and upload the .zip file to
  the Chrome Web Store.
