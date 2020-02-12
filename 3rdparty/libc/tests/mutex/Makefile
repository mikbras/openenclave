CC=/opt/musl/bin/musl-gcc
CFLAGS=-g -Wall -ftls-model=local-exec


all:
	$(MAKE) -C ../..
	$(CC) $(CFLAGS) -c main.c
	$(CC) $(CFLAGS) -static -o main main.o
	./main

clean:
	rm -rf main main.o
