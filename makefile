CC=gcc
CFLAGS=-Wall

numKey.o: numKey.c
	$(CC) $(CFLAGS) -c numKey.c

keygen.o: keygen.c
	$(CC) $(CFLAGS) -c keygen.c

otp_enc.o: otp_enc.c
	$(CC) $(CFLAGS) -c otp_enc.c

otp_enc_d.o: otp_enc_d.c
	$(CC) $(CFLAGS) -c otp_enc_d.c

keygen: keygen.o numKey.o
	$(CC) $(CFLAGS) -o keygen keygen.o numKey.o

otp_enc: otp_enc.o
	$(CC) $(CFLAGS) -o otp_enc otp_enc.o

otp_enc_d: otp_enc_d.o
	$(CC) $(CFLAGS) -o otp_enc_d otp_enc_d.o

clean:
	rm keygen *.o
