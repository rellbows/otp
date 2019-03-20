/* 
Program Name: otp_enc 
Description: Client script that sends a plaintext file and cipher key
to otp_enc_d and receives back encrypted, cipher text.
Author: Ryan Ellis
Class: CS344 - Operating Systems
Citations: 
-Used 'client.c' and 'server.c' sample code from 'Beej's Guide to
 Network Programming for to help in setting up connections/sending/receiving.
 http://beej.us/guide/bgnet/html/multi/clientserver.html
-Also used code from a previous project I worked on - chatserv_chatclient and 
 ftserv_ftclient.
 https://github.com/rellbows/chatserv_chatclient
 https://github.com/rellbows/ftserv_ftclient
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <arpa/inet.h>
#include "numKey.h"

#define CHUNKSIZE 1000

void *get_in_addr(struct sockaddr *sa);
int connection_setup(char *host, char *port);
int send_all(int sockfd, char *outgoing_buffer, int *total_buffer_size);
int get_msg_length(int sockfd);
ssize_t recv_all(int sockfd, char *msg, int msg_length);
off_t get_file_size(char* req_filename);
void get_file(FILE *input_fstream, char *input_file, size_t file_size);
void send_file(int data_sockfd, char *input_file, size_t file_size);
int check_keys(char *msg, int msg_length);

int main(int argc, char *argv[]){

	// holds socket descriptor setup by connection_setup()
	int sockfd = connection_setup("localhost", argv[1]);

	// for verifying otp_enc -> otp_enc_d
	char *otp_enc_msg = "8 otp_enc\0";
	int otp_enc_msg_size = 10;
	char *verify_status = NULL;
	int verify_status_length = 0;

	// for getting plaintext, cipherkey files
	char *plaintext_filename = argv[2];
	char *cipherkey_filename = argv[3];
	size_t plaintext_filesize, cipherkey_filesize;
	FILE *input_fstream = NULL;

	// holds plaintext, cipherkey, and ciphertext
	char *plaintext_msg = NULL;
	char *cipherkey_msg = NULL;
	char *ciphertext_msg = NULL;
	int ciphertext_msg_length = 0;

	// check to ensure 2 arguments passed thru command
	if(argc != 4){
		fprintf(stderr, "usage: port number\n");
		close(sockfd);
		exit(1);
	}

	// VERIFY otp_enc -> otp_enc_d
	send_all(sockfd, otp_enc_msg, &otp_enc_msg_size);

	// get response from server
	verify_status_length = get_msg_length(sockfd);
	
	if(verify_status_length == -1){
		close(sockfd);
		exit(1);		
	}

	verify_status = malloc(verify_status_length * sizeof(char));
	if(verify_status == NULL){
		fprintf(stderr, "Error allocating memory\n");
		close(sockfd);
		exit(1);
	}
	memset(verify_status, '\0', verify_status_length);
	recv_all(sockfd, verify_status, verify_status_length);

	// check verification status
	if(strcmp(verify_status, "FAIL\0") == 0){
		fprintf(stderr, "Error! Only \'otp_enc\' allowed to access \'otp_enc_d\'.\n");
		close(sockfd);
		exit(1);
	}
	
	// cleanup verify string
	free(verify_status);
	verify_status = NULL;

	// get plaintext file
	input_fstream = fopen(plaintext_filename, "r");

	if(input_fstream == NULL){
		fprintf(stderr, "Invalid filename!\n");
		close(sockfd);
		exit(1);
	}

	// get filesize of plaintext
	plaintext_filesize = get_file_size(plaintext_filename);

	// allocate memory to hold plaintext
	plaintext_msg = malloc(plaintext_filesize * sizeof(char));

	if(plaintext_msg == NULL){
		fprintf(stderr, "Error allocating memory!\n");
		close(sockfd);
		exit(1);
	}

	get_file(input_fstream, plaintext_msg, plaintext_filesize);
	
	// close plaintext filestream
	fclose(input_fstream);

	if(check_keys(plaintext_msg, plaintext_filesize) == -1){
		fprintf(stderr, "Invalid key in plaintext file.\n");
		close(sockfd);
		exit(1);
	}

	// get cipherkey file
	input_fstream = fopen(cipherkey_filename, "r");

	if(input_fstream == NULL){
		fprintf(stderr, "Invalid filename!\n");
		close(sockfd);
		exit(1);
	}

	// get filesize of cipherkey
	cipherkey_filesize = get_file_size(cipherkey_filename);

	// allocate memory to hold cipherkey
	cipherkey_msg = malloc(cipherkey_filesize * sizeof(char));

	if(cipherkey_msg == NULL){
		fprintf(stderr, "Error allocating memory!\n");
		close(sockfd);
		exit(1);
	}

	get_file(input_fstream, cipherkey_msg, cipherkey_filesize);
	
	// close cipherkey filestream
	fclose(input_fstream);

	if(plaintext_filesize > cipherkey_filesize){
		fprintf(stderr, "Error! Plaintext file is larger than cipherkey file.\n");
		close(sockfd);
		exit(1);
	}
	
	// send plaintext file
	send_file(sockfd, plaintext_msg, plaintext_filesize);
	send_file(sockfd, cipherkey_msg, cipherkey_filesize);

	// free memory allocated for plaintext
	free(plaintext_msg);
	plaintext_msg = NULL;
	
	// cleanup verify string
	free(verify_status);
	verify_status = NULL;

	// listen for verification of successful encryption
	verify_status_length = get_msg_length(sockfd);
	
	if(verify_status_length == -1){
		close(sockfd);
		exit(1);
	}

	verify_status = malloc(verify_status_length * sizeof(char));
	if(verify_status == NULL){
		fprintf(stderr, "Error allocating memory\n");
		close(sockfd);
		exit(1);
	}
	memset(verify_status, '\0', verify_status_length);
	recv_all(sockfd, verify_status, verify_status_length);

	// check encryption status
	if(strcmp(verify_status, "FAIL\0") == 0){
		fprintf(stderr, "Error during encryption! Invalid characters in plaintext file.\n");
		close(sockfd);
		exit(1);
	}
	
	// cleanup verify string
	free(verify_status);
	verify_status = NULL;

	// free memory allocated for plaintext
	free(cipherkey_msg);
	cipherkey_msg = NULL;

	return 0;
}

// Description: get sockaddr, IPv4 or IPv6
// Argument: socket address struct
// Returns: pointer to the address of the socket
void *get_in_addr(struct sockaddr *sa){
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Description: handles all preliminary connection setup
// Argument: string with destination host and port
// Returns: integer with sockfd needed for connection
int connection_setup(char *host, char *port){

	// for connection setup
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints); // clear the contents of the addrinfo struct
	hints.ai_family = AF_UNSPEC; // make the struct IP agnostic
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	// check results of call to getaddrinfo(), if not 0 then error.
	// if 0, then the required structs setup successfully and 
	// returns a pointer to a linked-lists of results
	if((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("client: socket");
			continue;
		}

		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	// checks connections
	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // free up memory allocated to servinfo	

	return sockfd;
}

/*
 * Description: get incoming msg length
 * Arugment(s): socket fd for connection
 * Returns: size of the incoming msg (without the prepended size)
 * Note: Can only return numbers with less than 19 or less digits.
*/
int get_msg_length(int sockfd){

	// var setup
	char str_msg_length[20];    // only holds numbers with 19or less digits
	int msg_length = 0;
	char digit[1];
	int i = 0;
	int numbytes;

	// get all the digits from the prepended msg length on msg
	numbytes = recv(sockfd, digit, 1, 0);

	if(numbytes == 0){
		fprintf(stderr, "Connection Close\n");
		return -1;
	}

	while(digit[0] != ' '){

		// check to ensure not going over 19 digit limit
		if(i >= 20){
			printf("Error! Message to to large.\n");
			exit(1);
		}

		// add to string
		str_msg_length[i] = digit[0];
		i++;
		
		// get next char
		recv(sockfd, digit, 1, 0);	
	}

	str_msg_length[++i] = '\0';

	msg_length = atoi(str_msg_length);

	if(msg_length == 0){
		printf("Error! Issue getting message length.\n");
		exit(1);
	}
	
	return msg_length;
}

/* Description: gets entire message
 * Arugments: the socket fd, string that holds the message, and msg length
 * Returns: length of msg
*/
ssize_t recv_all(int sockfd, char *msg, int msg_length){
	
	char recv_buffer[CHUNKSIZE];
	ssize_t chunk_length = 0;
	ssize_t total_chunk_length = 0;

	while(total_chunk_length < msg_length){

		// get chunk
		chunk_length = recv(sockfd, recv_buffer, CHUNKSIZE, 0);
	
		if(chunk_length == 0){
			fprintf(stderr, "Connection Closed\n");
			exit(1);
		}
	
		// move buffer to msg
		if(total_chunk_length == 0){
			strcpy(msg, recv_buffer);
		}
		else{
			strcat(msg, recv_buffer);
		}

		total_chunk_length += chunk_length;
	}

	return total_chunk_length;

}

// Description: sends the entire buffer
// Argument(s)s: socket fd for data connection, buffer, and total buffer size
// (with buffer length prepended)
// Returns: 0 if send completed, -1 if send failed
// Citations: Used Beej's 'Guide to Network Programming' section on sending
// partial segments of a buffer (Sect. 7.3)
// http://www.beej.us/guide/bgnet/html/multi/advanced.html#sendall
int send_all(int sockfd, char *outgoing_buffer, int *total_buffer_size){
	int total = 0;    // bytes sent
	int bytes_left = *total_buffer_size;    // how many bytes left
	int send_status;    // holds status of send (bytes sent when successful and -1 when not)

	while(total < *total_buffer_size){
		send_status = send(sockfd, outgoing_buffer + total, bytes_left, 0);
		if(send_status == -1){
			break;
		}
		total += send_status;
		bytes_left -= send_status;
	}

	*total_buffer_size = total;    // returns bytes send using pointer

	return send_status == -1 ? -1:0;    // conditional '?' operator, kind of like if/else
}

// Description: builds buffer to send working directory to client
// Argument(s): socket fd for data connection, array holding filenames and
// number of files in directory
// Returns: nothing
void send_file(int data_sockfd, char *input_file, size_t file_size){
	char *outgoing_buffer = NULL;
	int outgoing_buffer_size = file_size;
	int outgoing_buffer_size_size = 0;
	int total_buffer_size = 0;    // holds size of buffer + space needed for buffer size prefix
	char *str_outgoing_buffer_size = NULL;

	// get size needed for string holding buffer size
	outgoing_buffer_size_size = snprintf(NULL, 0, "%d", outgoing_buffer_size);
	outgoing_buffer_size_size++;    // add +1 for space delimiting buffer size

	total_buffer_size = outgoing_buffer_size + outgoing_buffer_size_size;

	// size of the outgoing buffer
	// plus enough space for digits holding the outgoing buffer size
	outgoing_buffer = malloc(sizeof(char) * total_buffer_size);

	// send message length to client
	// Citation: was having difficulties figuring out integer to string conversion
	// used below stack overflow thread as reference for prefixing packets with msg size
	// https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
	str_outgoing_buffer_size = malloc(sizeof(char) * outgoing_buffer_size_size);
	sprintf(str_outgoing_buffer_size, "%d", outgoing_buffer_size);
	strcpy(outgoing_buffer, str_outgoing_buffer_size);
	strcat(outgoing_buffer, " ");    // add delimiting space for buffer size

	// concatentate file contents into buffer
	strcat(outgoing_buffer, input_file);

	// send out buffer
	if(send_all(data_sockfd, outgoing_buffer, &total_buffer_size) == -1){
		perror("sendall");
		printf("Only %d bytes sent due to error!\n", total_buffer_size);
	}

	free(outgoing_buffer);
	free(str_outgoing_buffer_size);
}

// Description: gets size of the file
// Argument(s): filename of file requested
// Returns: size of the specified file
// Citations: used below stack overflow thread as reference
// https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
off_t get_file_size(char* req_filename){
	struct stat input_file_stat;    // holds stats on file

	if(stat(req_filename, &input_file_stat) == 0){
		return input_file_stat.st_size;
	}
	return -1;
}

// Description: get file content
// Argument(s): file stream object and char array to hold file contents
// Returns: nothing
// Citation: used below stack overflow thread for reference in how best to 
// grab file contents from file stream
void get_file(FILE *input_fstream, char *input_file, size_t file_size){
	size_t bytes_read = 0;    // holds bytes read by fread()

	// transfer data from stream to c string
	bytes_read = fread(input_file, 1, file_size, input_fstream);
	// check to ensure read correctly
	if(bytes_read != file_size){
		perror("fread");
	}
	// add null terminator
	input_file[file_size] = '\0';

}

// Description: checks input to key for invalid character (must be capital
// letters).
// Arguments: c string message to encrypt and its length
// Returns: 0 if keys are valid, otherwise -1
int check_keys(char *msg, int msg_length){

	int i;

	for(i = 0; i < msg_length - 1; i++){
		if(keyToNum(msg[i]) == -1){
			return -1;    // invalid character!
		}
	}
	return 0;
}

