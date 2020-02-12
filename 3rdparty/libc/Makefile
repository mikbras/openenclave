
all:
	( cd musl; make )
	( cd musl; make install )

clean:
	( cd musl; make clean )

distclean:
	( cd musl; make distclean )

configure:
	( cd musl; ./configure --prefix=/opt/musl --enable-debug=yes CFLAGS=-Werror )
