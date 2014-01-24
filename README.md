Mosh for Chrome
===============

Main author: Richard Woodbury (rpwoodbu@mybox.org)

This is a [Mosh](http://mosh.mit.edu) client port for the
[Chrome](http://www.google.com/chrome/) web browser using
[Native Client](https://developers.google.com/native-client). It is
particularly useful with [Chrome OS](http://www.google.com/chromeos).

Building
--------

  To build on Linux, just execute:

    $ ./build.sh

  This will download and build all dependencies. The directory "src/app" will
  contain all the assets necessary to run the client in the browser. Go to
  chrome://extensions, enable "Developer mode", and click the "Load unpacked
  extension button", directing it to the "app" directory. Then the app will be
  launchable from the app screen and the extensions screen.

  If there are problems with the build (particularly with building dependencies),
  you may need to delete the "build" directory to get a fresh start.

  To distribute the app, just upload the .zip file found in the "src/app"
  directory.

  The initial build will be very slow, as it has to download and build large
  dependencies. Subsequent builds are much faster. To go even faster while
  developing, including being able to build without updating dependencies (which
  requires Internet access), execute:

    $ ./build.sh fast

  This will not work if you have not had a successful build in the "slow" mode.

  To build on other platforms, you will need to get and setup the NaCl SDK and
  naclports yourself, and set `NACL_SDK_ROOT` and `NACL_PORTS` appropriately (see
  "build.sh" for hints). But all target platforms are cross-compiled from the one
  you are running, so there is no reason to build on other platforms, other than
  personal preference.
