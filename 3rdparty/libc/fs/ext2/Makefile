CFLAGS = -Wall -Werror

all:
	gcc $(CFLAGS) -c buf.c
	gcc $(CFLAGS) -c ext2.c
	gcc $(CFLAGS) -c main.c
	gcc -o main main.o buf.o ext2.o

clean:
	rm -f *.o
