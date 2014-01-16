#!/bin/bash -e

# Build the Native Client port of the Mosh client for running in the Chrome
# browser. If you have already built once and are doing active development on
# the Native Client port, invoke with the parameter "fast".

# Copyright 2013 Richard Woodbury
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

NACL_SDK_ZIP="nacl_sdk.zip"
NACL_SDK_URL="http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/${NACL_SDK_ZIP}"
NACL_SDK_DIR="nacl_sdk"
NACL_SDK_VERSION="pepper_32"

DEPOT_TOOLS_URL="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_DIR="depot_tools"

NACL_PORTS_URL="https://chromium.googlesource.com/external/naclports.git"
NACL_PORTS_DIR="naclports"

PROTOBUF_DIR="protobuf-2.5.0"
PROTOBUF_TAR="${PROTOBUF_DIR}.tar.bz2"
PROTOBUF_URL="https://protobuf.googlecode.com/files/${PROTOBUF_TAR}"

LIBSSH_DIR="libssh-0.6.0"
LIBSSH_TAR="${LIBSSH_DIR}.tar.xz"
LIBSSH_URL="https://red.libssh.org/attachments/download/71/${LIBSSH_TAR}"

INCLUDE_OVERRIDE="$(pwd)/src/include"

FAST=""
if [[ $# -gt 0 ]]; then
  FAST="$1"
  shift 1
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
    gclient sync
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
  make
  popd > /dev/null
fi
PROTO_PATH="$(pwd)/build/${PROTOBUF_DIR}/src"
export PATH="${PROTO_PATH}:${PATH}"
export LD_LIBRARY_PATH="${PROTO_PATH}/.libs"

# Get and patch (but not build) libssh.
if [[ ! -d "build/${LIBSSH_DIR}" ]]; then
  pushd "build" > /dev/null
  if [[ ! -f "${LIBSSH_TAR}" ]]; then
    wget "${LIBSSH_URL}"
  fi
  tar -xJf "${LIBSSH_TAR}"
  cd "${LIBSSH_DIR}"
  patch -p1 < ../../src/libssh.patch
  popd > /dev/null
fi

#export NACL_GLIBC="1"

pushd src > /dev/null
make clean
popd > /dev/null

for arch in x86_64 i686; do ( # Do all this in a separate subshell.
  export NACL_ARCH="${arch}"

  echo "Building packages in NaCl Ports..."
  pushd "${NACL_PORTS}/src" > /dev/null
  make ncurses zlib openssl protobuf
  popd > /dev/null

  echo "Updating submodules..."
  git submodule init
  git submodule update

  echo "Making hterm dist..."
  pushd deps/chromium_assets/chromeapps/hterm > /dev/null
  if [[ ! -d dist ]]; then
    bin/mkdist.sh
  fi
  popd > /dev/null

  echo "Loading naclports environment..."
  # For some reason I have to build NACLPORTS_LIBDIR myself, and I need vars to
  # do this that nacl_env.sh generates, so I end up calling that guy twice.
  . ${NACL_PORTS}/src/build_tools/nacl_env.sh
  export NACLPORTS_LIBDIR=${NACL_TOOLCHAIN_ROOT}/${NACL_CROSS_PREFIX}/usr/lib
  eval $(${NACL_PORTS}/src/build_tools/nacl_env.sh --print)

  glibc_compat="${NACL_TOOLCHAIN_ROOT}/${arch}-nacl/usr/include/glibc-compat"
  export CFLAGS="${CFLAGS} -I${glibc_compat}"
  export CXXFLAGS="${CXXFLAGS} -I${glibc_compat}"

  if [[ ${FAST} != "fast" ]]; then
    if [[ ! -d "build/${LIBSSH_DIR}/build-${arch}" ]]; then
      echo "Building libssh..."
      pushd "build/${LIBSSH_DIR}" > /dev/null
      rm -Rf "build-${arch}"
      mkdir "build-${arch}"
      cd "build-${arch}"
      cmake -DWITH_ZLIB=OFF -DWITH_STATIC_LIB=ON -DWITH_SHARED_LIB=OFF -DWITH_EXAMPLES=OFF -DHAVE_GETADDRINFO=ON ..
      make
      popd > /dev/null
    fi

    #
    # Mosh client build.
    #

    pushd deps/mosh > /dev/null
    if [[ ! -f configure ]]; then
      echo "Running autogen."
      ./autogen.sh
    fi
    popd > /dev/null # ..

    # Make a symlink into the usual include location so that the "override"
    # assert.h can find it. It changes for each port, and in unexpected ways,
    # which complicates things.
    include_arch="${arch}"
    if [[ "${include_arch}" == "i686" ]]; then
      include_arch="x86_64" # Yes, really.
    fi
    rm -f build/include
    ln -s "${NACL_TOOLCHAIN_ROOT}/${include_arch}-nacl/include" build/include

    build_dir="build/${NACL_ARCH}"
    mkdir -p "${build_dir}"
    pushd "${build_dir}" > /dev/null
    # Built-in functions cannot be overridden.
    export CXXFLAGS="${CXXFLAGS} -fno-builtin"
    if [[ "${NACL_GLIBC}" != "1" ]]; then
      # Do things specific to newlib.
      export CXXFLAGS="${CXXFLAGS} -I${INCLUDE_OVERRIDE} -DHAVE_FORKPTY -DHAVE_SYS_UIO_H"
    fi
    export LDFLAGS="${LDFLAGS} -Xlinker --unresolved-symbols=ignore-all"
    configure_options="--host=${arch} --enable-client=yes --enable-server=no --disable-silent-rules"
    if [[ "${arch}" == "i686" ]]; then
      # The i686 build doesn't seem to have stack protection, even though
      # "configure" finds it, so disabling hardening. :(
      configure_options="${configure_options} --disable-hardening"
    fi
    echo "Configuring..."
    ../../deps/mosh/configure ${configure_options}
    echo "Building Mosh with NaCl compiler..."
    make clean
    if [[ "${NACL_GLIBC}" == "1" ]]; then
      make || echo "*** Ignore error IFF it was the linking step. ***"
    else
      make
    fi
    popd > /dev/null # ${build_dir}
  fi

  pushd src > /dev/null
  target="app/mosh_client_${NACL_ARCH}.nexe"
  echo "Building ${target}..."
  make "${target}"
  popd > /dev/null # src

) done

pushd src > /dev/null
# Copy hterm dist files into app directory.
mkdir -p app/hterm
cp -f ../deps/chromium_assets/chromeapps/hterm/dist/js/* app/hterm

if [[ ${FAST} == "fast" ]]; then
  make nmf
else
  make all
fi
popd > /dev/null # src

echo "Done."
