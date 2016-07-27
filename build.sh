#!/bin/bash -e

# Build the Native Client port of the Mosh client for running in the Chrome
# browser. Only works in Linux, perhaps only in Ubuntu Linux.
#
# To build the dev track (suitable for most development testing):
#   $ ./build.sh dev
#
# To build the release track:
#   $ ./build.sh release
#
# To build an unoptimized debug version of the dev track (for use with gdb):
#   $ ./build.sh debug
#
# To run unit tests:
#   $ ./build.sh test


# Copyright 2016 Richard Woodbury
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

MODE="${1}"

FLAGS="--config=pnacl"
ACTION="build"

case "${MODE}" in
  "dev")
    TARGET="//:mosh_chrome_dev"
    ;;
  "release")
    TARGET="//:mosh_chrome"
    ;;
  "debug")
    TARGET="//:mosh_chrome_dev"
    FLAGS="-c dbg"
    ;;
  "test")
    ACTION="test"
    TARGET="..."
    FLAGS=""
    ;;
  *)
    echo "Unrecognized running mode." 1>&2
    echo "Usage: ${0} ( dev | release | debug | test ) [ bazel options ... ]" 1>&2
    exit 1
esac

if ! which bazel > /dev/null; then
  echo "The Bazel build tool was not found in your path." 1>&2
  echo "Go get it at: http://www.bazel.io/docs/install.html" 1>&2
  exit 1
fi

shift
bazel "${ACTION}" "${TARGET}" ${FLAGS} "$@"
