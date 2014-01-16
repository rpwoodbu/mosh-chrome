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
	ssh.cc

CXX_HEADERS:=\
	pepper_posix.h \
	pepper_posix_selector.h \
	pepper_posix_native_udp.h \
	pepper_posix_native_tcp.h \
	pepper_posix_tcp.h \
	pepper_posix_udp.h \
	pepper_wrapper.h \
	ssh.h

OBJECTS:=\
	../build/ARCH/src/frontend/mosh-client-nacl.o \
	../build/ARCH/src/frontend/stmclient.o \
	../build/ARCH/src/frontend/terminaloverlay.o

LIBDIRS:=\
	-L../build/ARCH/src/frontend \
	-L../build/ARCH/src/crypto \
	-L../build/ARCH/src/network \
	-L../build/ARCH/src/statesync \
	-L../build/ARCH/src/terminal \
	-L../build/ARCH/src/util \
	-L../build/ARCH/src/protobufs \
	-L../build/libssh-0.6.0/build-ARCH/src

LIBS:=\
	-lmoshcrypto \
	-lmoshnetwork \
	-lmoshstatesync \
	-lmoshterminal \
	-lmoshutil \
	-lmoshprotos \
	-lssh

OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_newlib)
CXX:=$(TC_PATH)/bin/i686-nacl-g++
OBJCOPY:=$(TC_PATH)/bin/i686-nacl-objcopy

# Project Build flags
override LDFLAGS+=-Xlinker --wrap=dup -Xlinker --wrap=_ssh_log -lppapi_cpp -lppapi -lz -lssl -lncurses -lprotobuf -lcrypto -lglibc-compat
# Add this linker flag to build statically:
	#-static -T $(TC_PATH)/x86_64-nacl/lib/ldscripts/elf64_nacl.x.static
#override WARNINGS+=-Wno-long-long -Wall -Wswitch-enum -Werror
#override CXXFLAGS+=-pthread -std=gnu++0x $(WARNINGS) -Iinclude \
        -I$(NACL_SDK_ROOT)/include
override CXXFLAGS+=-pthread $(WARNINGS) -I$(NACL_SDK_ROOT)/include -fno-builtin \
	-I../build/libssh-0.6.0/include
#override CFLAGS+=-pthread $(WARNINGS) -Iinclude -I$(NACL_SDK_ROOT)/include

#COMPAT_INC:=-I$(PNACL_TC_ROOT)/usr/include/glibc-compat
COMPAT_INC:=-I${TC_PATH}/ARCH-nacl/usr/include/glibc-compat
NEWLIB_CXXFLAGS:=$(CXXFLAGS) -DUSE_NEWLIB $(COMPAT_INC)
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))

# Declare the ALL target first, to make the 'all' target the default build
all: $(PROJECT).zip

# Convenience target for skipping the .zip file.
nmf: $(PROJECT).nmf

$(PROJECT).zip : $(PROJECT).nmf
	cd $(OUTDIR) && zip -r ../$(PROJECT).zip .

# Define 32 bit compile and link rules for C++ sources
i686_OBJS:=$(patsubst %.cc,%_32.o,$(CXX_SOURCES))
$(i686_OBJS) : %_32.o : %.cc $(THIS_MAKE) $(CXX_HEADERS)
	$(CXX) -o $@ -c $< -m32 $(i686_NEWLIB_CXXFLAGS)

../build/i686/src/frontend/mosh-client-nacl.o : ../build/i686/src/frontend/mosh-client.o
	$(OBJCOPY) --redefine-sym main=mosh_main $< $@

i686_MOSHOBJS:=$(subst ARCH,i686,$(OBJECTS))
i686_MOSHLIBDIRS:=$(subst ARCH,i686,$(LIBDIRS))
i686_NEWLIB_CXXFLAGS:=$(subst ARCH,i686,$(NEWLIB_CXXFLAGS))
$(PROJECT)_i686.nexe : $(i686_OBJS) $(i686_MOSHOBJS)
	$(CXX) -o $@ $^ -m32 $(LIBS) $(i686_NEWLIB_CXXFLAGS) \
	$(LDFLAGS) $(i686_MOSHLIBDIRS)

# Define 64 bit compile and link rules for C++ sources
x86_64_OBJS:=$(patsubst %.cc,%_64.o,$(CXX_SOURCES))
$(x86_64_OBJS) : %_64.o : %.cc $(THIS_MAKE) $(CXX_HEADERS)
	$(CXX) -o $@ -c $< -m64 $(x86_64_NEWLIB_CXXFLAGS)

../build/x86_64/src/frontend/mosh-client-nacl.o : ../build/x86_64/src/frontend/mosh-client.o
	$(OBJCOPY) --redefine-sym main=mosh_main $< $@

x86_64_MOSHOBJS:=$(subst ARCH,x86_64,$(OBJECTS))
x86_64_MOSHLIBDIRS:=$(subst ARCH,x86_64,$(LIBDIRS))
x86_64_NEWLIB_CXXFLAGS:=$(subst ARCH,x86_64,$(NEWLIB_CXXFLAGS))
$(PROJECT)_x86_64.nexe : $(x86_64_OBJS) $(x86_64_MOSHOBJS)
	$(CXX) -o $@ $^ -m64 $(LIBS) $(x86_64_NEWLIB_CXXFLAGS) \
	$(LDFLAGS) $(x86_64_MOSHLIBDIRS)

# Create NaCl Manifest
$(PROJECT).nmf : $(PROJECT)_x86_64.nexe $(PROJECT)_i686.nexe
	$(NACL_SDK_ROOT)/tools/create_nmf.py $^ -o $@ -s $(OUTDIR)

clean:
	rm -rf *.o $(PROJECT).zip $(PROJECT)*.nexe $(PROJECT).pexe \
	$(PROJECT).nmf $(OUTDIR)/lib* $(OUTDIR)/hterm
