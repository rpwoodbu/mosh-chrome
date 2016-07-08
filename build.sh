#!/bin/bash -e

# Build the Native Client port of the Mosh client for running in the Chrome
# browser. If you have already built once and are doing active development on
# the Native Client port, invoke with the parameter "fast". To do a release build,
# set the environment variable RELEASE to "true".

# Copyright 2013, 2014, 2015 Richard Woodbury
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

# Number of make jobs to run simultaneously.
JOBS=1
if [[ -x /usr/bin/nproc ]]; then
  # For N cores, Run N+1 make jobs.
  JOBS=$(($(/usr/bin/nproc) + 1))
fi

NACL_SDK_ZIP="nacl_sdk.zip"
NACL_SDK_URL="http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/${NACL_SDK_ZIP}"
NACL_SDK_DIR="nacl_sdk"
NACL_SDK_VERSION="pepper_45"

DEPOT_TOOLS_URL="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_DIR="depot_tools"

NACL_PORTS_URL="https://chromium.googlesource.com/external/naclports.git"
NACL_PORTS_DIR="naclports"
NACL_PORTS_REV=4112c10

PROTOBUF_VERSION="2.6.0"
PROTOBUF_DIR="protobuf-${PROTOBUF_VERSION}"
PROTOBUF_TAR="${PROTOBUF_DIR}.tar.bz2"
PROTOBUF_URL="https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/${PROTOBUF_TAR}"

LIBSSH_DIR="libssh-0.7.1"
LIBSSH_TAR="${LIBSSH_DIR}.tar.gz"
LIBSSH_URL="https://git.libssh.org/projects/libssh.git/snapshot/libssh-0.7.1.tar.gz"

INCLUDE_OVERRIDE="$(pwd)/src/include"

FAST=""
if [[ $# -gt 0 ]]; then
  FAST="$1"
  shift 1
fi

if [[ "${FAST}" == "fast" && -v RELEASE ]]; then
  echo "Refusing to do a fast release build." 1>&2
  exit 1
fi

if [[ ! -d "build" ]]; then
  mkdir -p build
fi

# Get NaCl SDK.
if [[ "$(uname)" != "Linux" && "${NACL_SDK_ROOT}" == "" ]]; then
  echo "Please set NACL_SDK_ROOT. Auto-setup of this is only on Linux."
  exit 1
fi
if [[ "${NACL_SDK_ROOT}" == "" ]]; then
  if [[ ! -d "build/${NACL_SDK_DIR}" ]]; then
    pushd build > /dev/null
    wget "${NACL_SDK_URL}"
    unzip "${NACL_SDK_ZIP}"
    popd > /dev/null
  fi
  if [[ "${FAST}" != "fast" ]]; then
    pushd "build/${NACL_SDK_DIR}"
    ./naclsdk update "${NACL_SDK_VERSION}"
    popd > /dev/null
  fi
  export NACL_SDK_ROOT="$(pwd)/build/${NACL_SDK_DIR}/${NACL_SDK_VERSION}"
fi

# Get depot_tools. If not on Linux, skip this and expect ${NACL_PORTS} to be
# set.
if [[ "$(uname)" == "Linux" ]]; then
  if [[ ! -d "build/${DEPOT_TOOLS_DIR}" ]]; then
    pushd build > /dev/null
    git clone "${DEPOT_TOOLS_URL}"
    popd > /dev/null
  fi
  export PATH="${PATH}:$(pwd)/build/${DEPOT_TOOLS_DIR}"
fi

# Get NaCl Ports.
if [[ "$(uname)" != "Linux" && "${NACL_PORTS}" == "" ]]; then
  echo "Please set NACL_PORTS. Auto-setup of this is only on Linux."
  exit 1
fi
if [[ "${NACL_PORTS}" == "" ]]; then
  if [[ ! -d "build/${NACL_PORTS_DIR}" ]]; then
    mkdir -p "build/${NACL_PORTS_DIR}"
    pushd "build/${NACL_PORTS_DIR}" > /dev/null
    gclient config --name=src "${NACL_PORTS_URL}"
    popd > /dev/null
  fi
  if [[ "${FAST}" != "fast" ]]; then
    pushd "build/${NACL_PORTS_DIR}" > /dev/null
    gclient sync --revision ${NACL_PORTS_REV}
    popd > /dev/null
  fi
  export NACL_PORTS="$(pwd)/build/${NACL_PORTS_DIR}"
fi

# Get and build protoc to match what's in NaCl Ports.
if [[ ! -d "build/${PROTOBUF_DIR}" ]]; then
  mkdir -p "build/${PROTOBUF_DIR}"
  pushd "build" > /dev/null
  wget "${PROTOBUF_URL}"
  tar -xjf "${PROTOBUF_TAR}"
  cd "${PROTOBUF_DIR}"
  ./configure
  make -j${JOBS}
  popd > /dev/null
fi
PROTO_PATH="$(pwd)/build/${PROTOBUF_DIR}/src"
export PATH="${PROTO_PATH}:${PATH}"
export LD_LIBRARY_PATH="${PROTO_PATH}/.libs"
# Ensure that Mosh's configure script doesn't try to do anything stupid with
# these.
export protobuf_CFLAGS=" "
export protobuf_LIBS=" "

# Get and patch (but not build) libssh.
if [[ ! -d "build/${LIBSSH_DIR}" ]]; then
  pushd "build" > /dev/null
  if [[ ! -f "${LIBSSH_TAR}" ]]; then
    wget "${LIBSSH_URL}"
  fi
  tar -xzf "${LIBSSH_TAR}"
  cd "${LIBSSH_DIR}"
  # Apply patch for NACL.
  patch -p1 < ../../src/libssh.patch
  # Apply patch for SSH certs.
  patch -p1 < ../../src/libssh-cert.patch
  popd > /dev/null
fi

#export NACL_GLIBC="1"

pushd src > /dev/null
make clean
popd > /dev/null

export NACL_ARCH="pnacl"
export TOOLCHAIN="pnacl"

echo "Building packages in NaCl Ports..."
pushd "${NACL_PORTS}/src" > /dev/null
make ncurses zlib openssl protobuf
popd > /dev/null

echo "Updating submodules..."
git submodule sync
git submodule init
git submodule update

pushd deps/libapps/hterm > /dev/null
if [[ ${FAST} != "fast" ]]; then
  echo "Making hterm dist..."
  bin/mkdist.sh
fi
popd > /dev/null

echo "Loading naclports environment..."
. ${NACL_PORTS}/src/build_tools/nacl-env.sh

# Make PNaCl tools available.
export CC=${NACLCC}
export CXX=${NACLCXX}
export AR=${NACLAR}
export RANLIB=${NACLRANLIB}
export PKG_CONFIG_LIBDIR=${NACL_PREFIX}/lib/pkgconfig

glibc_compat="${NACL_PREFIX}/include/glibc-compat"
include_flags="-I${glibc_compat} -I${NACL_SDK_ROOT}/include/pnacl -I${NACL_SDK_ROOT}/include"
export CFLAGS="${CFLAGS} ${include_flags}"
export CXXFLAGS="${CXXFLAGS} ${include_flags}"

libssh="build/${LIBSSH_DIR}/build/src/libssh.a"
if [[ ! -f "${libssh}" ]]; then
  echo "Building libssh..."
  pushd "build/${LIBSSH_DIR}" > /dev/null
  rm -Rf "build"
  mkdir "build"
  cd "build"
  cmake -DPNACL=ON -DWITH_ZLIB=OFF -DWITH_STATIC_LIB=ON -DWITH_SHARED_LIB=OFF -DWITH_EXAMPLES=OFF -DHAVE_GETADDRINFO=ON ..
  make
  popd > /dev/null
  ${RANLIB} "${libssh}"
fi

if [[ ${FAST} != "fast" ]]; then
  #
  # Mosh client build. Do in a subshell to avoid side effects.
  #

  (
    pushd deps/mosh > /dev/null
    if [[ ! -f configure ]]; then
      echo "Running autogen."
      ./autogen.sh
    fi
    popd > /dev/null # ..

    # Make a symlink into the usual include location so that the "override"
    # assert.h can find it. It changes for each port, and in unexpected
    # ways, which complicates things.
    rm -f build/include
    ln -s "${NACL_PREFIX}/../include" build/include

    build_dir="build/mosh"
    mkdir -p "${build_dir}"
    pushd "${build_dir}" > /dev/null
    # Built-in functions cannot be overridden.
    export CXXFLAGS="${CXXFLAGS} -fno-builtin"
    # Tell Mosh we're building for NaCl (changes main to mosh_main).
    export CXXFLAGS="${CXXFLAGS} -DNACL"
    # Tell Clang to handle C++ exceptions, if badly.
    # TODO: Once PNaCl supports exceptions properly, remove this flag.
    export CXXFLAGS="${CXXFLAGS} --pnacl-exceptions=sjlj"

    if [[ "${NACL_GLIBC}" != "1" ]]; then
      # Do things specific to newlib.
      export CXXFLAGS="${CXXFLAGS} -I${INCLUDE_OVERRIDE} -DHAVE_FORKPTY -DHAVE_SYS_UIO_H -O2"
    fi
    # Do configure step in a subshell as we need to fake it out a little.
    (
      export CFLAGS="${CFLAGS} -include ${INCLUDE_OVERRIDE}/ncurses_stub.h"
      configure_options="--host=pnacl --enable-client=yes --enable-server=no --disable-silent-rules"
      echo "Configuring..."
      ../../deps/mosh/configure ${configure_options}
    )
    echo "Building Mosh with NaCl compiler..."
    make clean
    make -j${JOBS} || echo "*** Ignore error IFF it was the linking step. ***"
    popd > /dev/null # ${build_dir}
  )
fi

pushd src > /dev/null
# Copy hterm dist files into app directory.
mkdir -p app/hterm
cp -f ../deps/libapps/hterm/dist/js/* app/hterm
make -e -j${JOBS} all
popd > /dev/null # src

echo "Done."
