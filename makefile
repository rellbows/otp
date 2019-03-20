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

otp_dec.o: otp_dec.c
	$(CC) $(CFLAGS) -c otp_dec.c

otp_dec_d.o: otp_dec_d.c
	$(CC) $(CFLAGS) -c otp_dec_d.c

keygen: keygen.o numKey.o
	$(CC) $(CFLAGS) -o keygen keygen.o numKey.o

otp_enc: otp_enc.o numKey.o
	$(CC) $(CFLAGS) -o otp_enc otp_enc.o numKey.o

otp_enc_d: otp_enc_d.o numKey.o
	$(CC) $(CFLAGS) -o otp_enc_d otp_enc_d.o numKey.o

otp_dec: otp_dec.o numKey.o
	$(CC) $(CFLAGS) -o otp_dec otp_dec.o numKey.o

otp_dec_d: otp_dec_d.o numKey.o
	$(CC) $(CFLAGS) -o otp_dec_d otp_dec_d.o numKey.o

clean:
	rm otp_enc otp_enc_d otp_dec otp_dec_d keygen *.o
