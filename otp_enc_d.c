/*
** Filename: otp_end_d.c
** Description: Daemon script that takes plaintext and a cipher key sent
** over a socket connection from otp_enc script and encrypts the plaintext
** Author: Ryan Ellis
** Class: CS344 - Operating Systems
** Citations:
** -Most of the boilerplate connection code is based of the 'server.
** c' from 'beejs guide to network programming'
** http://beej.us/guide/bgnet/
** -Used an existing project of mine for alot of the code as well - ftserver
**  ftclient
**  https://github.com/rellbows/ftserv_ftclient
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "numKey.h"

#define BACKLOG 10    // how many pending connections queue will hold
#define CHUNKSIZE 1000    // size of chunks that can be received

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int connection_setup(char *serv_port);
int get_msg_length(int sockfd);
ssize_t recv_all(int sockfd, char *msg, int msg_length);
int send_all(int sockfd, char *outgoing_buffer, int *total_buffer_size);
void send_file(int data_sockfd, char *input_file, size_t file_size);
int encrypt(char *plaintext_msg, int plaintext_msg_length, char *cipherkey_msg, char *ciphertext_msg);

int main(int argc, char *argv[]){

	// silence warning for argc
	(void)argc;

	// variables for control connection setup
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	char *serv_port;    // server port to listen on
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];

	// variables for msg's 
	char *verify_msg = NULL;
	int verify_msg_length = 0;
	char *fail_msg = "5 FAIL\0";
	int fail_msg_length = 7;
	char *pass_msg = "5 PASS\0";
	int pass_msg_length = 7;
	char *plaintext_msg = NULL;    // holds unencrypted msg
	int plaintext_msg_length = 0;
	char *ciphertext_msg = NULL;    // holds encrypted msg
	int ciphertext_msg_length = 0;    // holds size of msg's
	char *cipherkey_msg = NULL;
	int cipherkey_msg_length = 0;

	// get port num for server to run on from cmd line arg
	serv_port = argv[1];

	// setup control connection
	sockfd = connection_setup(serv_port);

	printf("Server open on %s\n", serv_port);

	while(1){

		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

		if(new_fd == -1){
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);

		printf("Connection from %s\n", s);

		if (!fork()) { // this is the child proces!)s
			close(sockfd); // child doesn't need the listener

			// VERIFYING CLIENT PROG -> otp_enc
			// get length of verifying msg
			verify_msg_length = get_msg_length(new_fd);

			if(verify_msg_length == -1){
				close(new_fd);
			}

			// allocate enough mem for verify msg
			verify_msg = malloc(verify_msg_length * sizeof(char));
			memset(verify_msg, '\0', verify_msg_length * sizeof(char));
		
			// check allocation
			if(verify_msg == NULL){
				printf("Error allocating memory.\n");
				return 1;
			}

			// get verify msg
			recv_all(new_fd, verify_msg, verify_msg_length);

			// check verify msg
			if(strcmp(verify_msg, "otp_enc\0") != 0){
				send_all(new_fd, fail_msg, &fail_msg_length);
			}
			else{
				send_all(new_fd, pass_msg, &pass_msg_length);
			}

			// cleanup verify string
			free(verify_msg);
			verify_msg = NULL;

			// get length of plaintext msg incoming
			plaintext_msg_length = get_msg_length(new_fd);

			if(plaintext_msg_length == -1){
				close(new_fd);
			}

			// allocate enough mem for plaintext msg
			plaintext_msg = malloc(plaintext_msg_length * sizeof(char));
			memset(plaintext_msg, '\0', plaintext_msg_length * sizeof(char));
			
			// check allocation
			if(plaintext_msg == NULL){
				printf("Error allocating memory.\n");
				return 1;
			}
			
			// get plaintext msg
			recv_all(new_fd, plaintext_msg, plaintext_msg_length);

			// get length of cipherkey msg incoming
			cipherkey_msg_length = get_msg_length(new_fd);

			// allocate enough mem for plaintext msg
			cipherkey_msg = malloc(cipherkey_msg_length * sizeof(char));
			memset(cipherkey_msg, '\0', cipherkey_msg_length * sizeof(char));
			
			// check allocation
			if(cipherkey_msg == NULL){
				printf("Error allocating memory.\n");
				return 1;
			}
			
			// get cipherkey msg
			recv_all(new_fd, cipherkey_msg, cipherkey_msg_length);

			// encode msg
			ciphertext_msg_length = plaintext_msg_length;
			
			ciphertext_msg = malloc(ciphertext_msg_length * sizeof(char));
			memset(ciphertext_msg, '\0', ciphertext_msg_length * sizeof(char));

			// check allocation
			if(ciphertext_msg == NULL){
				printf("Error allocating memory.\n");
				return 1;
			}

			if(encrypt(plaintext_msg, plaintext_msg_length, cipherkey_msg, ciphertext_msg) == 1){
				send_all(new_fd, fail_msg, &fail_msg_length);
				close(new_fd);
			}
			else{
				send_all(new_fd, pass_msg, &pass_msg_length);
			}

			send_file(new_fd, ciphertext_msg, ciphertext_msg_length);
			
			// cleanup plaintext msg
			free(plaintext_msg);
			plaintext_msg = NULL;

			// cleanup cipherkey msg
			free(cipherkey_msg);
			cipherkey_msg = NULL;
	
			// cleanup ciphertext msg
			free(ciphertext_msg);
			ciphertext_msg = NULL;

			close(new_fd);    // close out control connection

			exit(0);	
		}
		
		close(new_fd);
	}

	return 0;
}

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
 	
	while(waitpid(-1, NULL, WNOHANG) > 0);
	
	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Description: handles all preliminary connection setup of control connection
// Argument(s): string with port
// Returns: integer with sockfd needed for connection
int connection_setup(char *serv_port){

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, serv_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

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
		fprintf(stderr, "Connection Closed\n");
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
int send_all(int data_sockfd, char *outgoing_buffer, int *total_buffer_size){
	int total = 0;    // bytes sent
	int bytes_left = *total_buffer_size;    // how many bytes left
	int send_status;    // holds status of send (bytes sent when successful and -1 when not)

	while(total < *total_buffer_size){
		send_status = send(data_sockfd, outgoing_buffer + total, bytes_left, 0);
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
	int outgoing_buffer_size = file_size + 1;    // +1 for null terminator
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

// Description: Encrypts plaintext file using cipherkey provided
// Arguments: The plaintext msg, the length of the plaintext, the cipherkey,
// and space to hold the ciphertext
// Returns: Returns 1 if invalid character was used in the plaintext, otherwise 0
int encrypt(char *plaintext_msg, int plaintext_msg_length, char *cipherkey_msg, char *ciphertext_msg){

	// var setup
	int i;    // traveler
	int plain_key_num, cipher_key_num, plain_cipher_total;
	char ciphertext_char;

	for(i = 0; i < plaintext_msg_length - 1; i++){
		
		// get plaintext num
		if((plain_key_num = keyToNum(plaintext_msg[i])) == -1){
			printf("Error! Key not found.\n");
			return 1;
		}

		// get cipherkey num
		if((cipher_key_num = keyToNum(cipherkey_msg[i])) == -1){
			printf("Error! Key not found.\n");
			return 1;
		}

		// total of plaintext cipher nums
		plain_cipher_total = cipher_key_num + plain_key_num;

		if(plain_cipher_total > 26){
			plain_cipher_total -= 27;
		}
	
		// get char for encrypted key
		ciphertext_char = numToKey(plain_cipher_total);

		ciphertext_msg[i] = ciphertext_char;

	}

	ciphertext_msg[i] = '\n';

	return 0;
}
