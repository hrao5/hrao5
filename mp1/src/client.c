/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 2048 // max number of bytes we can get at once 
#define REQUESTLENGTH 1024

void parse_url(char* url, char** hostname, char** port, char** path) {
    printf("URL: %s\n", url);
    char* p;
    p = strstr(url, "://");

    char* protocol = 0;
    if (p) {
        protocol = url;
        *p = 0;
        p += 3;
    } else {
        p = url;
    }

    if (protocol) {
        if (strcmp(protocol, "http")) {
            fprintf(stderr, "unkown protocol '%s', only 'http' is supported.\n", protocol);
            exit(1);
        }
    }

    *hostname = p;
    while (*p && *p != ':' && *p != '/' && *p != '#') ++p;

    *port = "80";
    if (*p == ':') {
        *p++ = 0;
        *port = p;
    }
    while (*p && *p != '/' && *p != '#') ++p;

    *path = p;
    if (*p == '/') {
        *path = p+1;
    }
    *p = 0;

    while (*p && *p != '#') ++p;
    if (*p == '#') *p = 0;

    printf("hostname: %s\n", *hostname);
    printf("port: %s\n", *port);
    printf("path: %s\n", *path);
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
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	memset(buf, 0, MAXDATASIZE);
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char *hostname, *port, *path;
	parse_url(argv[1], &hostname, &port, &path);

	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// edit request
	char request[REQUESTLENGTH];
	memset(request, 0, REQUESTLENGTH);
	snprintf(request, 1023, "GET /%s HTTP/1.1\r\n\r\n", path);

	send(sockfd, request, strlen(request), 0);	// send request
	printf("sent request: %s", request);

	shutdown(sockfd, 1);
	// if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	//     perror("recv");
	//     exit(1);
	// }

	// buf[numbytes] = '\0';
	
	int output = open("output", O_CREAT|O_TRUNC|O_RDWR|O_APPEND, 0777);
	// int output = creat("output.txt", 0777);
	if (output == -1) {
		printf("error open: %s\n", strerror(errno));
	}
	printf("line153\n");

	int header = 0;
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) > 0) {
		if (!header) {
			char* body = strstr(buf, "\r\n\r\n")+4;
			write(output, body, numbytes-(body - buf));
			printf("body: %s", body);
			header = 1;
		} else {
			write(output, buf, numbytes);
			printf("%s", buf);
		}
		memset(buf, 0, MAXDATASIZE);
	}

	// printf("client: received '%s'\n",buf);
	close(output);
	close(sockfd);

	return 0;
}

