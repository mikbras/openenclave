INCLUDES += -I/root/openenclave/include

#LDFLAGS += -L/root/openenclave/build/output/lib/openenclave/enclave -loecore

all: musl/config.mak
	( cd musl; make )
	( cd musl; make install )

clean:
	( cd musl; make clean )

distclean:
	( cd musl; make distclean )

musl/config.mak:
	( cd musl; ./configure --prefix=/opt/musl --disable-shared --enable-debug=yes CFLAGS="-Werror -fPIE $(INCLUDES) $(LDFLAGS)" )
