
#
# This file is part of SushiVM
# Copyright (c) 2019-2021 J42 Pte Ltd
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
	argutil.o \
	apps.o \
	sushi.o \
	install_application.o \
	lmarshal.o \
	zbuf.o \
	lib_crypto.o \
	lib_io.o \
	lib_net.o \
	lib_os.o \
	lib_util.o \
	lib_vm.o \
	lib_mod.o \
	lib_zip.o \
	lib_image.o \
	zlib/deflate.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/zutil.o zlib/trees.o zlib/adler32.o zlib/crc32.o \
	minizip/unzip.o minizip/zip.o minizip/ioapi.o \
	lib_bcrypt.o \
	lib_math.o \
	$(OBJS_SYSDEP)
CC=$(CC_SYSDEP)
CFLAGS=-Iluajit/src -Izlib -Iminizip -DSUSHI_VERSION=\"$(VERSION)\" -DSUSHI_SUPPORT_ZLIB $(CFLAGS_SYSDEP)
LDFLAGS=$(LDFLAGS_SYSDEP)
ifeq ($(STATIC_BUILD),yes)
	LDFLAGS += -static
endif
LIBS=$(LIBS_SYSDEP)

all: sushi test

### Libcrypt

libcrypt:
	make CC="$(CC)" -C libbcrypt/ all

### OpenSSL

ifeq (Darwin,$(LUAJIT_TARGET_SYS))
libssl:
	if [ ! -f openssl/build/libssl.a ]; then \
		(cd openssl && tar xvf openssl-1.1.1d.tar.gz && rm -rf build && mv openssl-1.1.1d build && cd build && ./config no-shared && make) ; \
	fi
else
libssl:
	echo Not building openssl
endif

### Libpng

ifeq (Windows,$(LUAJIT_TARGET_SYS))
libpng:
	if [ ! -f png/build/.libs/libpng16.a ]; then \
		(cd png && tar xvf libpng-1.6.37.tar.gz && rm -rf build && mv libpng-1.6.37 build && cp config.cache build && cd build && ./configure --prefix=$(shell pwd)/png/build --host=x86_64-w64-mingw32 --cache-file=config.cache CPPFLAGS=-I$(shell pwd)/zlib --enable-static --disable-shared && make libpng16.la LIBS="-lm") ; \
	fi
endif
ifeq (Linux,$(LUAJIT_TARGET_SYS))
libpng:
	if [ ! -f png/build/.libs/libpng16.a ]; then \
		(cd png && tar xvf libpng-1.6.37.tar.gz && rm -rf build && mv libpng-1.6.37 build && cp config.cache build && cd build && ./configure --prefix=$(shell pwd)/png/build --cache-file=config.cache CPPFLAGS=-I$(shell pwd)/zlib --enable-static --disable-shared && make libpng16.la LIBS="-lm") ; \
	fi
endif
ifeq (Darwin,$(LUAJIT_TARGET_SYS))
libpng:
	if [ ! -f png/build/.libs/libpng16.a ]; then \
		(cd png && tar xvf libpng-1.6.37.tar.gz && rm -rf build && mv libpng-1.6.37 build && cp config.cache build && cd build && ./configure --prefix=$(shell pwd)/png/build --cache-file=config.cache CPPFLAGS=-I$(shell pwd)/zlib --enable-static --disable-shared && make libpng16.la LIBS="-lm") ; \
	fi
endif

### LuaJIT

ifeq (Darwin,$(LUAJIT_TARGET_SYS))
libluajit:
	make MACOSX_DEPLOYMENT_TARGET="$(MACOS_GCC_VERSION)" CC="$(CC)" HOST_CC="$(HOST_CC_SYSDEP)" TARGET_SYS="$(LUAJIT_TARGET_SYS)" Q= -C luajit/src libluajit.a
else
libluajit:
	make CC="$(CC)" HOST_CC="$(HOST_CC_SYSDEP)" TARGET_SYS="$(LUAJIT_TARGET_SYS)" Q= -C luajit/src libluajit.a
endif

install_application.lz: install_application.ss
	sushi ss-0.17 -o install_application.lz -c install_application.ss

install_application.c: install_application.lz
	sushi binary_to_code.ss install_application.lz install_application_lz install_application.c

sushi: libluajit libssl libcrypt libpng $(OBJS)
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
	rm -f install_application.c install_application.lz
	make -C luajit/src clean
	make -C libbcrypt/ clean
	rm -rf release
	rm -rf openssl/build
	rm -rf png/build
	rm -f $(OBJS) sushi sushi.exe
