CC=/opt/musl/bin/musl-gcc
CFLAGS += -g
CFLAGS += -Wall
#CFLAGS += -fPIC
CFLAGS += -O0
#CFLAGS += -ftls-model=local-exec
#CFLAGS += -ftls-model=global-dynamic
#CFLAGS += -ftls-model=local-dynamic
#CFLAGS += -ftls-model=initial-exec

all:
	$(MAKE) -C ../..
	$(CC) $(CFLAGS) -c main.c
	$(CC) $(CFLAGS) -static -o main main.o
	./main

clean:
	rm -rf main main.o
