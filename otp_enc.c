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

#include <arpa/inet.h>

#define CHUNKSIZE 1000

void *get_in_addr(struct sockaddr *sa);
int connection_setup(char *host, char *port);
int send_all(int sockfd, char *outgoing_buffer, int *total_buffer_size);

int main(int argc, char *argv[]){

	// holds socket descriptor setup by connection_setup()
	int sockfd = connection_setup("localhost", argv[1]);

	// for confirming otp_enc -> otp_enc_d
	char *otp_enc_msg = "8 otp_enc\0";
	int otp_enc_msg_size = 10;

	// buffer for receiving data
	char *recv_buffer[CHUNKSIZE];
	int num_bytes;

	// check to ensure 2 arguments passed thru command
	if(argc != 2){
		fprintf(stderr, "usage: port number\n");
		exit(1);
	}

	// send id to server
	send_all(sockfd, otp_enc_msg, &otp_enc_msg_size);

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
	int sockfd, numbytes;
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
