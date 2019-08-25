
#
# This file is part of SushiVM
# Copyright (c) 2019 Eqela Oy
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

include Makefile.config

VERSION=0.99.0
OBJS=main.o \
	sushi.o \
	lmarshal.o \
	zbuf.o \
	lib_crypto.o \
	lib_io.o \
	lib_net.o \
	lib_os.o \
	lib_util.o \
	lib_vm.o \
	$(OBJS_SYSDEP)
CC=$(CC_SYSDEP)
CFLAGS=-Iluajit/src -DSUSHI_VERSION=\"$(VERSION)\" $(CFLAGS_SYSDEP)
LDFLAGS=$(LDFLAGS_SYSDEP)
LIBS=$(LIBS_SYSDEP)

all: sushi

libluajit:
	make CC="$(CC)" HOST_CC="$(HOST_CC_SYSDEP)" TARGET_SYS="$(LUAJIT_TARGET_SYS)" Q= -C luajit/src libluajit.a

sushi: libluajit $(OBJS)
	$(CC) -o sushi$(EXESUFFIX) $(OBJS) luajit/src/libluajit.a $(LDFLAGS) $(LIBS)

clean:
	make -C luajit/src clean
	rm -f $(OBJS) sushi sushi.exe
