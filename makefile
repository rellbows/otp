CC=gcc
CFLAGS=-std=c99 -Wall

numKey.o: numKey.c
	$(CC) $(CFLAGS) -c numKey.c

keygen.o: keygen.c
	$(CC) $(CFLAGS) -c keygen.c

keygen: keygen.o numKey.o
	$(CC) $(CFLAGS) -o keygen keygen.o numKey.o

otp_enc_d: otp_enc_d.c numKey.h socket_helpers.h
	$(CC) -o otp_end_d otp_end_d.c numKey.h socket_helpers.h 

clean:
	rm keygen *.o
