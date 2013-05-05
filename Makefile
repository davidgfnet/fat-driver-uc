
CFLAGS = -ggdb 

all:
	gcc -c fat.c $(CFLAGS)
	gcc -c main.c $(CFLAGS)
	gcc -o fat_reader main.o fat.o

clean:
	rm -f *.o fat_reader
