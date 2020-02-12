MUSLDIR=/opt/musl

CC=gcc

CFLAGS += -g
CFLAGS += -Wall
CFLAGS += -fPIE
CFLAGS += -fno-stack-protector
CFLAGS += -Werror
CFLAGS += -nostdinc

INCLUDES = -I$(MUSLDIR)/include

LDFLAGS += -nostdlib
LDFLAGS += -nodefaultlibs
LDFLAGS += -nostartfiles
LDFLAGS += -L$(MUSLDIR)/lib
LDFLAGS += $(MUSLDIR)/lib/libpthread.a
#LDFLAGS += $(MUSLDIR)/lib/crt1.o
LDFLAGS += $(MUSLDIR)/lib/libc.a
LDFLAGS += $(MUSLDIR)/lib/librt.a

all:
	$(MAKE) -C ../..
	$(CC) $(CFLAGS) $(INCLUDES) -c main.c
	$(CC) $(CFLAGS) -o main main.o $(LDFLAGS)
	./main

clean:
	rm -rf main main.o
