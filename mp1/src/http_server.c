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

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 1024

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

long HTTP_OK(char* buf, FILE* fp, char* http)
{
  memset(buf, 0, MAXDATASIZE);
  strncpy(buf, http, 8);
  char msg[11] = " 200 OK\r\n\r\n";
  strncpy(buf+8, msg, 11);

  fseek(fp, 0, SEEK_END);
  long lSize = ftell(fp);
  // printf("File Size: %lu\n", lSize);
  rewind(fp);
  if (lSize+19 < MAXDATASIZE)
      fread(buf+19, 1, lSize, fp);
  else
      fread(buf+19, 1, MAXDATASIZE-19, fp);
  return lSize+19;
}

void HTTP_ERROR(char* buf, char* http)
{
  memset(buf, 0, MAXDATASIZE);
  char msg[MAXDATASIZE] = " 404 Not Found\r\n\r\nYour file was not found in the system";
  strncpy(buf, http, 8);
  strcpy(buf+8, msg);
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int send_loop(FILE* fp, int new_fd, char* buf, long fSize, int numBytes)
{
    while (numBytes < fSize)
    {
        memset(buf, 0, MAXDATASIZE);
        int sendBytes = 0;
        int sendStatus = 0;
        if (fSize-numBytes < MAXDATASIZE)
        {
          fread(buf, 1, fSize-numBytes, fp);
          sendBytes = fSize-numBytes;
        }
        else
        {
            fread(buf, 1, MAXDATASIZE, fp);
            sendBytes = MAXDATASIZE;
        }

        if ((sendStatus = send(new_fd, buf, sendBytes, 0)) == -1)
        {
            perror("send");
            exit(1);
        }

        numBytes += sendStatus;
    }
    return numBytes;
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if (argc != 2) {
	    fprintf(stderr,"usage: server port number\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	// printf("PORT: %s\n", argv[1]);	

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
			char buf[MAXDATASIZE];
			char filename[MAXDATASIZE];
			char getter[3];
			char http[8];
			memset(buf, 0, MAXDATASIZE);
                        memset(filename, 0, MAXDATASIZE);
			int numbytes = 0;
			if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
	    			perror("recv");
	    			exit(1);
			}
			int begdex = 4;
			int enddex = 4+search_char(buf+4, ' ');
			strncpy(filename, buf+5, enddex-begdex-1);
			strncpy(getter, buf, 3);
			strncpy(http, buf+enddex+1, 8);

			// printf("FILENAME: %s\n", filename);
                        // printf("REQUEST: %s\n", buf);        
			FILE *fp;
                        long fSize = 0;
			fp = fopen(filename, "rb");
			if (fp == NULL)
                        {
                          HTTP_ERROR(buf, http);
                          fSize = strlen(buf);
                        }
			else
                          fSize = HTTP_OK(buf, fp, http);
                        if (fSize < MAXDATASIZE)
                            numbytes = send(new_fd, buf, fSize, 0);
                        else
                            numbytes = send(new_fd, buf, MAXDATASIZE, 0);
                        numbytes = send_loop(fp, new_fd, buf, fSize, numbytes);
                        /*
                        if ((numbytes = send(new_fd, buf, MAXDATASIZE, 0)) == -1)
                        {
                          perror("send");
                          exit(1);
                        }
                        */
                        // printf("RESPONSE: %s\n", buf);
                        fclose(fp);
                        close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}
	return 0;
}

