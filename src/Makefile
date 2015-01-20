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
PROJECT:=$(OUTDIR)/mosh_client
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

# Project Build flags
# TODO: Remove --pnacl-exceptions once exceptions are fully supported by PNaCl.
override CXXFLAGS+=-pthread $(WARNINGS) -I$(NACL_SDK_ROOT)/include -fno-builtin \
	--pnacl-exceptions=sjlj -O2

COMPAT_INC:=-I${TC_PATH}/ARCH-nacl/usr/include/glibc-compat
NEWLIB_CXXFLAGS:=$(CXXFLAGS) -DUSE_NEWLIB $(COMPAT_INC)

# Declare the ALL target first, to make the 'all' target the default build
all: $(PROJECT).zip

# Convenience target for skipping the .zip file.
nmf: $(PROJECT).nmf

$(PROJECT).zip : $(PROJECT).nmf
	cd $(OUTDIR) && zip -r ../$(PROJECT).zip .

# Define PNaCl compile and link rules for C++ sources
OBJS:=$(patsubst %.cc,%.o,$(CXX_SOURCES))
$(OBJS) : %.o : %.cc $(CXX_HEADERS)
	$(CXX) -o $@ -c $< $(NEWLIB_CXXFLAGS)

../build/mosh/src/frontend/mosh-client-nacl.o : ../build/mosh/src/frontend/mosh-client.o
	$(OBJCOPY) --redefine-sym main=mosh_main $< $@

$(PROJECT).pexe : $(OBJS) $(OBJECTS)
	$(CXX) -o $@ $^ $(LIBS) $(NEWLIB_CXXFLAGS) $(LDFLAGS) $(LIBDIRS)
	$(TC_PATH)/bin/pnacl-finalize $@

# Create NaCl Manifest
$(PROJECT).nmf : $(PROJECT).pexe
	$(NACL_SDK_ROOT)/tools/create_nmf.py $^ -o $@ -s $(OUTDIR)

clean:
	rm -rf *.o $(PROJECT).zip $(PROJECT)*.nexe $(PROJECT).pexe \
	$(PROJECT).nmf $(OUTDIR)/lib* $(OUTDIR)/hterm
