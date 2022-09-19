/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define DATASIZE 1024

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }
	if (argc != 2) {
		fprintf(stderr,"usage: ./http_server port\n");
	    exit(1);
	}

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

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

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);
		
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			// if (send(new_fd, "Hello, world!", 13, 0) == -1)
			// 	perror("send");
			int num_read;
			char request[1024];
			memset(request, 0, 1024);

			if ((num_read = recv(new_fd, request, 1023, 0)) == -1) {
				printf("recv err: %s", strerror(errno));
				exit(1);
			}

			request[num_read] = '\0';

			char* req = strtok(request, " ");
			if (strcmp(req, "GET" ) != 0) {
                send(new_fd, "HTTP/1.1 400 Bad Request\r\n\r\n", strlen("HTTP/1.1 400 Bad Request\r\n\r\n"), 0);
				exit(1);
			}
			char* path = strtok(NULL, " ");
			path += 1;
			char* protocol = strtok(NULL, "\0");

			printf("path: %s\n", path);
			if (access(path, F_OK) == -1) {
				send(new_fd, "HTTP/1.1 404 Not Found\r\n\r\n", strlen("HTTP/1.1 404 Not Found\r\n\r\n"), 0);
				exit(1);
			} else {
				FILE* file = fopen(path, "rb");
				if (!file) {
					send(new_fd, "HTTP/1.1 403 Forbidden\r\n\r\n", strlen("HTTP/1.1 403 Forbidden\r\n\r\n"), 0);
					exit(1);
				} else {
					memset(request, 0, 1024);
					char reply[] = "HTTP/1.1 200 OK\r\n\r\n";
					strcpy(request, reply);
					int len = strlen(reply);

					while ((num_read = fread(request+len, sizeof(char), 1023-len, file)) != 0 ){
						request[len+num_read] = '\0';

						if ((num_read = send(new_fd, request, strlen(request), 0)) == -1) {
							printf("send err: %s", strerror(errno));
							exit(1);
						}
						memset(request,0,sizeof(request));
						len = 0;
					}
				}
				fclose(file);
			}		
			close(new_fd);
			exit(0);

			// 		send(new_fd, "HTTP/1.1 200 OK\r\n\r\n", strlen("HTTP/1.1 200 OK\r\n\r\n"), 0);
			// 		memset(request, 0, 1024);
			// 		size_t len = 0;
			// 		while(getline((char**) &request, &len, file) != -1) {
			// 			send(new_fd, request, 1024, 0);
			// 			printf("buf: %s", request);
			// 			memset(request, 0, 1024);
			// 		}
			// 		printf("err: %s", strerror(errno));
			// 	}
			// }

			// close(new_fd);
			// exit(0);
		}
		
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

