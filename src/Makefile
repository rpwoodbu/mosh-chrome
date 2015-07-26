# Copyright 2013, 2014 Richard Woodbury
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

OUTDIR:=app
NAME:=mosh_client
PROJECT:=$(OUTDIR)/$(NAME)
CXX_SOURCES:=\
	mosh_nacl.cc \
	pepper_posix.cc \
	pepper_posix_native_udp.cc \
	pepper_posix_native_tcp.cc \
	pepper_posix_selector.cc \
	pepper_posix_tcp.cc \
	pepper_posix_udp.cc \
	pepper_wrapper.cc \
	ssh.cc \
	ssh_login.cc

CXX_HEADERS:=\
	mosh_nacl.h \
	pepper_posix.h \
	pepper_posix_selector.h \
	pepper_posix_native_udp.h \
	pepper_posix_native_tcp.h \
	pepper_posix_tcp.h \
	pepper_posix_udp.h \
	pepper_wrapper.h \
	pthread_locks.h \
	ssh.h \
	ssh_login.h

OBJECTS:=\
	../build/mosh/src/frontend/mosh-client.o \
	../build/mosh/src/frontend/stmclient.o \
	../build/mosh/src/frontend/terminaloverlay.o

LIBDIRS:=\
	-L$(NACL_SDK_ROOT)/lib/pnacl/Release \
	-L../build/mosh/src/frontend \
	-L../build/mosh/src/crypto \
	-L../build/mosh/src/network \
	-L../build/mosh/src/statesync \
	-L../build/mosh/src/terminal \
	-L../build/mosh/src/util \
	-L../build/mosh/src/protobufs

LIBS:=\
	-lmoshcrypto \
	-lmoshnetwork \
	-lmoshstatesync \
	-lmoshterminal \
	-lmoshutil \
	-lmoshprotos \
	-lssh \
	-lppapi_cpp \
	-lppapi \
	-lz \
	-lssl \
	-lncurses \
	-lprotobuf \
	-lcrypto \
	-lglibc-compat

OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_pnacl)
CXX:=$(TC_PATH)/bin/pnacl-clang++
OBJCOPY:=$(TC_PATH)/../$(OSNAME)_x86_newlib/bin/i686-nacl-objcopy
ENVSUBST:=envsubst
APP_NAME:=Mosh
DESCRIPTION:=Mosh (mobile shell) for Chrome
RELEASE:=false

# Project Build flags
# TODO: Remove --pnacl-exceptions once exceptions are fully supported by PNaCl.
# NB: --pnacl-exceptions=sjlj seems to upset the debugger to the point of being
# useless. So when debugging, remove that options. This will cause all
# exceptions to abort(), unfortunately.
override CXXFLAGS+=-pthread $(WARNINGS) -I$(NACL_SDK_ROOT)/include \
	-fno-builtin -std=gnu++11 \
	--pnacl-exceptions=sjlj -O2
  # When debugging, replace the line above with the line below:
  # -g3 -fno-omit-frame-pointer

COMPAT_INC:=-I${TC_PATH}/ARCH-nacl/usr/include/glibc-compat
NEWLIB_CXXFLAGS:=$(CXXFLAGS) -DUSE_NEWLIB $(COMPAT_INC)

# Declare the ALL target first, to make the 'all' target the default build
all: $(PROJECT).zip

# Convenience target for skipping the .zip file.
nozip: $(PROJECT).nmf $(OUTDIR)/manifest.json

$(PROJECT).zip: $(PROJECT).nmf $(OUTDIR)/manifest.json
	cd $(OUTDIR) && zip -r ../$(PROJECT).zip .

# Define PNaCl compile and link rules for C++ sources
OBJS:=$(patsubst %.cc,%.o,$(CXX_SOURCES))
$(OBJS) : %.o : %.cc $(CXX_HEADERS)
	$(CXX) -o $@ -c $< $(NEWLIB_CXXFLAGS)

$(NAME).bc: $(OBJS) $(OBJECTS)
	$(CXX) -o $@ $^ $(LIBS) $(NEWLIB_CXXFLAGS) $(LDFLAGS) $(LIBDIRS)

$(NAME).pexe: $(NAME).bc
	$(TC_PATH)/bin/pnacl-finalize -o $@ $<

$(PROJECT)_x86_64.nexe: $(NAME).pexe
	$(TC_PATH)/bin/pnacl-translate -arch x86-64 -o $@ $<

$(PROJECT)_i686.nexe: $(NAME).pexe
	$(TC_PATH)/bin/pnacl-translate -arch i686 -o $@ $<

# ARM translation seems broken; see Issue #90. Setting -threads=seq at least
# prevents the file size from varying, and _may_ reduce the number of incidents
# of broken nexes.
$(PROJECT)_armv7.nexe: $(NAME).pexe
	$(TC_PATH)/bin/pnacl-translate -arch armv7 -threads=seq -o $@ $<

# Create NaCl Manifest
$(PROJECT).nmf: $(PROJECT)_x86_64.nexe $(PROJECT)_i686.nexe $(PROJECT)_armv7.nexe
	$(NACL_SDK_ROOT)/tools/create_nmf.py $^ -o $@ -s $(OUTDIR)

# Generate manifest.json based on dev or release mode.
$(OUTDIR)/manifest.json: build_number.txt
	export NAME="$(APP_NAME)"; \
	export DESCRIPTION="$(DESCRIPTION)"; \
	export BUILD_NUM=$$(cat $^); \
	if [ "$(RELEASE)" = "false" ]; then \
	  NAME="$${NAME} (dev)"; \
		DESCRIPTION="$${DESCRIPTION} (dev track)"; \
	fi; \
	$(ENVSUBST) '$$NAME $$DESCRIPTION $$BUILD_NUM' < manifest.json.template > $@

# Increment build number.
build_number.txt: FORCE
	if [ ! -f $@ ]; then \
		echo "0" > $@; \
	else \
		echo $$(($$(cat $@) + 1)) > $@; \
	fi

.PHONY: FORCE
FORCE:

# Purposefully do not clean up build_number.txt.
clean:
	rm -rf *.o $(PROJECT).zip $(PROJECT)*.nexe $(PROJECT).pexe $(PROJECT).bc \
	$(PROJECT).nmf $(OUTDIR)/lib* $(OUTDIR)/hterm $(OUTDIR)/manifest.json \
	*.bc *.pexe
