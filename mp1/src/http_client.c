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

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 1024 // max number of bytes we can get at once 
#define BLOCKSIZE 1024

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int search_char(char* s, char c)
{
	int i = 0;
	for (i = 0; i < strlen(s); ++i)
	{
		if (s[i] == c)
			return i;
	}
	return -1;
}

int receive_loop(FILE* fp, int sockfd)
{
    char buf[BLOCKSIZE];
    int numbytes, totalbytes;
    int guardcheck = 0;
    /*
    if (numbytes = recv(sockfd, buf, BLOCKSIZE, 0) == -1)
    {
        perror("recv");
        exit(1);
    }
    */
    while ((numbytes = recv(sockfd, buf, BLOCKSIZE, 0)) > 0)
    {
        totalbytes += numbytes;
        if (guardcheck == 1)
            fwrite(buf, 1, numbytes, fp);
        else
        {
            char* first = strstr(buf, "\r\n\r\n");
            if (first != NULL)
            {
                guardcheck = 1;
                first += 4;
                int index = (int)(first-buf);
                fwrite(first, 1, numbytes-index, fp);
            }
        }
    }

    if (numbytes < 0)
    {
        perror("recv");
        exit(1);
    }

    return totalbytes;
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
    char buf[MAXDATASIZE];
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

	char hostname[MAXDATASIZE];
	char port[6] = "80";
	memset(hostname, 0, MAXDATASIZE);
	int host_end = search_char(argv[1]+7, '/')+7;
	int port_dex = search_char(argv[1]+7, ':')+7;
	if (port_dex < host_end && port_dex != 6)
            strncpy(port, argv[1]+port_dex+1, (host_end-port_dex-1));
        else
            port_dex = host_end;
	strncpy(hostname, argv[1]+7, (port_dex-7));
	int payload_len = strlen(argv[1]+host_end);
	printf("Host: %s\n", hostname); 
	printf("Node: %s\n", argv[1]);
	printf("Port: %s\n", port);
	printf("Payload Length: %d\n", payload_len);

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

	char* http_header = (char*)malloc(sizeof(char)*(payload_len+strlen(hostname)+25));
	memset(http_header, 0, payload_len+strlen(hostname)+27);
	strncpy(http_header, "GET ", 5);
	strncpy(http_header+4, argv[1]+host_end, payload_len);
	strncpy(http_header+4+payload_len, " HTTP/1.1\r\n", 12);
	strncpy(http_header+15+payload_len, "Host: ", 7);
	strncpy(http_header+21+payload_len, hostname, strlen(hostname));
	strncpy(http_header+21+payload_len+strlen(hostname), "\r\n\r\n", 5);


	printf("%s", http_header);
	//printf("%d", http_header[strlen(http_header)-1]);
	
	if ((numbytes = send(sockfd, http_header, 25+payload_len+strlen(hostname), 0)) == -1)
	{
	    perror("send");
	    exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}
    
	buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

//    close(sockfd);
	free(http_header);

	FILE *fp;
	fp = fopen("output", "wb");
	if (fp == NULL)
	{
	    perror("Error: ");
	    exit(1);
	}	

    fputs(buf, fp);
    receive_loop(fp, sockfd);
    close(sockfd);
    fclose(fp);
	return 0;
}

