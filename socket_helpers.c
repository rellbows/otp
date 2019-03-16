/*
** Filename: socket_helpers.c
** Description: Source file for socket connection helpers.
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

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG 10    // how many pending connections queue will hold

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	waitpid() might overwrite errno, so we save and restore it:
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
int control_setup(char *serv_port){

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof hints);
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

// Description: handles all preliminary connection setup of data connection
// Argument(s): string with destination host and port
// Returns: integer with sockfd needed for connection
int data_setup(char *host, char *port){

	// for connection setup
	int sockfd;
	//int numbytes;
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
