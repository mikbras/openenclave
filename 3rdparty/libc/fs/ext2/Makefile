CFLAGS = -Wall -Werror

all:
	gcc $(CFLAGS) -c buf.c
	gcc $(CFLAGS) -c ext2.c
	gcc $(CFLAGS) -c blkdev.c
	gcc $(CFLAGS) -c bitmap.c
	gcc $(CFLAGS) -c main.c
	gcc -o main main.o buf.o ext2.o blkdev.o bitmap.o

clean:
	rm -f *.o main
