CFLAGS=-fPIC

all: mod_hello.so

mod_hello.so: mod_hello.o
	$(CC) mod_hello.o -shared -o mod_hello.so

clean:
	rm -f *.o *.so
