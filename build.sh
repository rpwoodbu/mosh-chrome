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
# To build the Windows "native" installers:
#   $ ./build.sh windows-x64
#   $ ./build.sh windows-ia32
#
# To build an unoptimized debug version of the dev track (for use with gdb):
#   $ ./build.sh debug
#
# To run unit tests:
#   $ ./build.sh test
#
# To lint all C++ files:
#   $ ./build.sh lint
#
# To do a clang-format pass over the code:
#   $ ./build.sh format


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
GIT_ROOT="$(git rev-parse --show-toplevel)"

format() {
  CLANG_FORMAT="clang-format-4.0"
  if ! which "${CLANG_FORMAT}" > /dev/null; then
    echo "${CLANG_FORMAT} not in the path. Get it at: http://apt.llvm.org/" 1>&2
    exit 1
  fi
  cd "${GIT_ROOT}"
  find . -name '*.cc' -or -name '*.h' -or -name '*.js' | xargs "${CLANG_FORMAT}" -i
}

FLAGS=""
ACTION="build"

case "${MODE}" in
  "dev")
    TARGET="//:mosh_chrome_dev"
    ;;
  "release")
    TARGET="//:mosh_chrome"
    ;;
  "windows-x64")
    TARGET="//windows:mosh_windows_x64"
    ;;
  "windows-ia32")
    TARGET="//windows:mosh_windows_ia32"
    ;;
  "debug")
    TARGET="//:mosh_chrome_dev"
    FLAGS="${FLAGS} -c dbg"
    ;;
  "test")
    ACTION="test"
    TARGET="..."
    ;;
  "lint")
    ACTION="run"
    TARGET="@styleguide//:cpp_lint"
    FLAGS="${FLAGS} -- $(find ${GIT_ROOT} -name '*.cc' -or -name '*.h')"
    ;;
  "format")
    format
    exit 0
    ;;
  *)
    echo "Unrecognized running mode." 1>&2
    echo "Usage: ${0} ( dev | release | windows-(x64|ia32) | debug | test ) [ bazel options ... ]" 1>&2
    echo "       ${0} ( lint | format )" 1>&2
    exit 1
esac

shift
set -x
./bazelisk "${ACTION}" "${TARGET}" ${FLAGS} "$@"
