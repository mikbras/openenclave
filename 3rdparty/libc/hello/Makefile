CC=/opt/musl/bin/musl-gcc
CFLAGS=-fPIE -pie
OE_LIBS=/root/openenclave/build/output/lib/openenclave/enclave

export LD_LIBRARY_PATH=/opt/musl/lib

all:
	$(CC) $(CFLAGS) -c hello.c
	$(CC) $(CFLAGS) -o hello $(OE_LIBS)/liboecore.a hello.o

foo:
	$(CC) -fPIC -shared -o libfoo.so foo.c
	file libfoo.so
	ldd libfoo.so

run:
	./hello

ldd:
	ldd ./hello

clean:
	rm -f *.so *.o
