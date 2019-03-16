CC=gcc
CFLAGS=-std=c99 -Wall
DEPS = numKey.h
OBJ = numKey.o keygen.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

keygen: keygen.o numKey.o
	$(CC) -o keygen keygen.o numKey.o

clean:
	rm keygen *.o
