
#
# This file is part of SushiVM
# Copyright (c) 2019-2020 Eqela Oy
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

ifeq ($(OS),Windows_NT)
	include Makefile.config.win32
else
	ifeq ($(CROSSTARGET),win32)
		include Makefile.config.win32
	else
		uname_S := $(shell uname -s)
		ifeq ($(uname_S), Linux)
			include Makefile.config.linux
		endif
		ifeq ($(uname_S), Darwin)
			include Makefile.config.macos
		endif
	endif
endif

ifeq ($(VERSION),)
	VERSION=`date '+%Y%m%d'`
endif
OBJS=main.o \
	sushi.o \
	lmarshal.o \
	zbuf.o \
	lib_bcrypt.o \
	lib_crypto.o \
	lib_io.o \
	lib_net.o \
	lib_os.o \
	lib_util.o \
	lib_vm.o \
	lib_mod.o \
	lib_zip.o \
	zlib/deflate.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/zutil.o zlib/trees.o zlib/adler32.o zlib/crc32.o \
	minizip/unzip.o minizip/zip.o minizip/ioapi.o \
	$(OBJS_SYSDEP)
CC=$(CC_SYSDEP)
CFLAGS=-Iluajit/src -Izlib -Iminizip -DSUSHI_VERSION=\"$(VERSION)\" -DSUSHI_SUPPORT_ZLIB $(CFLAGS_SYSDEP)
LDFLAGS=$(LDFLAGS_SYSDEP)
ifeq ($(STATIC_BUILD),yes)
	LDFLAGS += -static
endif
LIBS=$(LIBS_SYSDEP)
BCRYPTAFILES=$(BCRYPT_A_FILES)

all: sushi test

ifeq (Windows,$(BCRYPT_TARGET_SYS))
libcrypt:
	echo Not building libbcrypt
else
libcrypt:
	make -C libbcrypt/ all
endif

ifeq (Darwin,$(LUAJIT_TARGET_SYS))
libssl:
	if [ ! -f openssl/build/libssl.a ]; then \
		(cd openssl && tar xvf openssl-1.1.1d.tar.gz && rm -rf build && mv openssl-1.1.1d build && cd build && ./config no-shared && make) ; \
	fi
else
libssl:
	echo Not building openssl
endif

ifeq (Darwin,$(LUAJIT_TARGET_SYS))
libluajit:
	make MACOSX_DEPLOYMENT_TARGET="$(MACOS_GCC_VERSION)" CC="$(CC)" HOST_CC="$(HOST_CC_SYSDEP)" TARGET_SYS="$(LUAJIT_TARGET_SYS)" Q= -C luajit/src libluajit.a
else
libluajit:
	make CC="$(CC)" HOST_CC="$(HOST_CC_SYSDEP)" TARGET_SYS="$(LUAJIT_TARGET_SYS)" Q= -C luajit/src libluajit.a
endif

sushi: libluajit libssl libcrypt $(OBJS)
	$(CC) -o sushi$(EXESUFFIX) $(OBJS) luajit/src/libluajit.a libbcrypt/bcrypt.a $(LDFLAGS) $(LIBS)

test:
	@if [ "$(CROSSTARGET)" = "" ]; then \
		if ! ./sushi test.lua; then echo "[FAILED] Tests failed to execute successfully"; exit 1; fi; \
	else \
		echo "Skipping tests when cross-compiling."; \
	fi

release: sushi
	$(STRIP_SYSDEP) sushi$(EXESUFFIX)
	rm -rf release
	mkdir -p release
	zip -j release/sushi_$(VERSION)_$(PLATFORMNAME).zip sushi$(EXESUFFIX) ../LICENSE ../README.md

clean:
	make -C luajit/src clean
	make -C libbcrypt/ clean
	rm -rf release
	rm -rf openssl/build
	rm -f $(OBJS) sushi sushi.exe
