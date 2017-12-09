#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "support.h"
#include "Server.h"
#include <ctype.h>
#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>
using namespace std;
void sendData(char * buf, size_t size, int fd);
void help(char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("Initiate a network file server\n");
	printf("  -m    enable multithreading mode\n");
	printf("  -l    number of entries in the LRU cache\n");
	printf("  -p    port on which to listen for connections\n");
}

void sendData(char * buf, int fd){
	//printf("%s\n","send" );
  size_t n = strlen(buf);
  size_t nremain = n;
  size_t nsofar;
  char *bufp = buf;
  // printf("sending: %s",strbuf);
  while (nremain > 0) {
    if ((nsofar = write(fd, bufp, nremain)) <= 0) {
      if (errno != EINTR) {

        fprintf(stderr, "Write error: %s\n", strerror(errno));
        exit(0);
      }
      nsofar = 0;
			printf("%s\n", "here" );
    }
    nremain -= nsofar;
    bufp += nsofar;
  }
}





char* try_put(char* request, int size){
	int loc= 0;
	int idx =0;
	int data_start = 0;
	char * to_return = (char *)malloc(sizeof(char)*1024);
	const int MAXLINE = 8192;
	char filesize[MAXLINE];
	char put_name[MAXLINE];
     for(int i= 0; i<size; i++){
			 if(request[i] == '\n'){
				 loc = i + 1;
				 break;
			 }
		 }
		 while(isdigit(request[loc]) && request[loc] != '\n'){
			 filesize[idx] = request[loc];
			 idx++;
			 loc++;
		 }
		 filesize[idx + 1] = '\0';
data_start = loc +1;
stringstream str(filesize);
int x;
str >> x;
//printf("%d\n", x);
idx = 0;
int name_start = 4;
for(int i = name_start; i<size; i++){
	if(request[i]=='\n'){
		loc = i + 1;
		break;
	}else{
		put_name[idx] = request[i];
		idx++;
	}
}
 put_name[idx] = '\0';
char * contents;
contents = (char *)malloc((x)*sizeof(char)); // Enough memory for file + \0

memcpy(contents, request+data_start, x);
//printf("%s\n", put_name);
//printf("%s\n", contents);
if (access(put_name, F_OK) != -1){
	//printf("%s\n", "file exists... overwrite" );
	if(FILE *fp = fopen(put_name, "wb")){
	fwrite(contents, sizeof(char), (x), fp);
	fclose(fp);
	printf("File Updated: %s. Contents:-->%s" , put_name, contents);
	strcpy(to_return, "OK\n");
}
else{
	printf("%s%s\n","Error Acessing File: " , put_name);
	strcpy(to_return, "Error Accessing File on Server.\n");
}
}
else{
	if(FILE *fp = fopen(put_name, "wb")){
	fwrite(contents, sizeof(char), (x), fp);
	fclose(fp);
	printf("File Created: %s. Contents:-->%s" , put_name, contents);
	strcpy(to_return, "OK\n");
}
else{
	printf("%s%s\n","Error Acessing File: " , put_name);
	strcpy(to_return, "Error Accessing File on Server.\n");
}
}


return to_return;
}




void die(const char *msg1, char *msg2)
{
	fprintf(stderr, "%s, %s\n", msg1, msg2);
	exit(0);
}

/*
 * open_server_socket() - Open a listening socket and return its file
 *                        descriptor, or terminate the program
 */
int open_server_socket(int port)
{
	int                listenfd;    /* the server's listening file descriptor */
	struct sockaddr_in addrs;       /* describes which clients we'll accept */
	int                optval = 1;  /* for configuring the socket */

	/* Create a socket descriptor */
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		die("Error creating socket: ", strerror(errno));
	}

	/* Eliminates "Address already in use" error from bind. */
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
	{
		die("Error configuring socket: ", strerror(errno));
	}

	/* Listenfd will be an endpoint for all requests to the port from any IP
	   address */
	bzero((char *) &addrs, sizeof(addrs));
	addrs.sin_family = AF_INET;
	addrs.sin_addr.s_addr = htonl(INADDR_ANY);
	addrs.sin_port = htons((unsigned short)port);
	if(bind(listenfd, (struct sockaddr *)&addrs, sizeof(addrs)) < 0)
	{
		die("Error in bind(): ", strerror(errno));
	}

	/* Make it a listening socket ready to accept connection requests */
	if(listen(listenfd, 1024) < 0)  // backlog of 1024
	{
		die("Error in listen(): ", strerror(errno));
	}

	return listenfd;
}

/*
 * handle_requests() - given a listening file descriptor, continually wait
 *                     for a request to come in, and when it arrives, pass it
 *                     to service_function.  Note that this is not a
 *                     multi-threaded server.
 */
void handle_requests(int listenfd, void (*service_function)(int, int), int param, bool multithread)
{
	while(1)
	{
		/* block until we get a connection */
		struct sockaddr_in clientaddr;
		memset(&clientaddr, 0, sizeof(sockaddr_in));
		socklen_t clientlen = sizeof(clientaddr);
		int connfd;
		if((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0)
		{
			die("Error in accept(): ", strerror(errno));
		}

		/* print some info about the connection */
		struct hostent *hp;
		hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if(hp == NULL)
		{
			fprintf(stderr, "DNS error in gethostbyaddr() %d\n", h_errno);
			exit(0);
		}
		char *haddrp = inet_ntoa(clientaddr.sin_addr);
		printf("server connected to %s (%s)\n", hp->h_name, haddrp);

		/* serve requests */
		service_function(connfd, param);

		/* clean up, await new connection */
		if(close(connfd) < 0)
		{
			die("Error in close(): ", strerror(errno));
		}
	}
}

char * getData(int connfd){

		const int MAXLINE = 8192;
		char   *   buf =(char*) malloc (sizeof(char)* MAXLINE);   /* a place to store text from the client */
		//bzero(buf, MAXLINE);


		/* read from socket, recognizing that we may get short counts */
		char *bufp = buf;              /* current pointer into buffer */
		ssize_t nremain = MAXLINE;     /* max characters we can still read */
		size_t nsofar;                 /* characters read so far */
		while (1)
		{
		//	fprintf(stderr, "%s\n", "stuck" );
			/* read some data; swallow EINTRs */
			if((nsofar = read(connfd, bufp, nremain)) < 0)
			{
				printf("%s\n","error" );
				if(errno != EINTR)
				{
					die("read error: ", strerror(errno));
				}
				continue;
			}
			/* end service to this client on EOF */
			if(nsofar == 0)
			{

				//fprintf(stderr, "%s\n", buf );
				fprintf(stderr, "received EOF\n");
				return buf;
				//die("fdsjkhl", "shjf");

			}

						bufp += nsofar;
						nremain -= nsofar;

			if(*(bufp-1) == '\n'){
				*bufp = 0;
				break;
				//break;
			}
			/* update pointer for next bit of reading */


		}
	  printf("server received %d bytes\n", MAXLINE-nremain);
		return buf;

}

/*
 * file_server() - Read a request from a socket, satisfy the request, and
 *                 then close the connection.
 */
void file_server(int connfd, int lru_size){
	char* result = (char *)malloc(sizeof(char)*8192);

	/* TODO: set up a few static variables here to manage the LRU cache of
	   files */

	/* TODO: replace following sample code with code that satisfies the
	   requirements of the assignment */
		 while (1) {
		 const int MAXLINE = 8192;
		 char* filename = (char *)malloc(sizeof(char)* MAXLINE);
	 	char* fsize = (char *)malloc(sizeof(char)* MAXLINE);

		 char      buf[MAXLINE];   /* a place to store text from the client */
		 bzero(buf, MAXLINE);

		 /* read from socket, recognizing that we may get short counts */
		 char *bufp = buf;              /* current pointer into buffer */
		 ssize_t nremain = MAXLINE;     /* max characters we can still read */
		 size_t nsofar;

		// int nline = 0;              /* characters read so far */
		 while (1) {
			 /* read some data; swallow EINTRs */
			 if ((nsofar = read(connfd, bufp, nremain)) < 0) {
				 if (errno != EINTR)
				 die("read error: ", strerror(errno));
				 continue;
			 }
			 /* end service to this client on EOF */
			 if (nsofar == 0) {
				 fprintf(stderr, "received EOF\n");
				 return;
			 }
			 /* update pointer for next bit of reading */
			 bufp += nsofar;
			 nremain -= nsofar;
			 if (*(bufp-1) == '\n') {
				// nline ++;
				 *bufp = 0;
				 break;
			 }
		 }


		 /* dump content back to client (again, must handle short counts) */
		 printf("server received %d bytes\n", MAXLINE-nremain);
		 nremain = bufp - buf;
		// bufp = buf;
		//printf("%s",bufp);
		/* dump content back to client (again, must handle short counts) */

		//fprintf(stderr,  buf );
		if(strncmp(buf, "PUT", 3) ==0){
			//fprintf(stderr, "This is a put.\n" );
			memcpy(filename, buf + 4, strlen(buf) - 4);
			//printf("%s\n",buf );
			//printf("%s\n", filename );
			fsize = getData(connfd);
			//printf("%s\n", fsize );

			memcpy(buf + strlen(buf), fsize, strlen(fsize));
			int size_as_dec = 0;
			int len = strlen(fsize);
			for(int i=0; i<len; i++){
				size_as_dec = size_as_dec * 10 + (fsize[i] - '0');
			}
			char* contents = (char *)malloc(sizeof(char)* size_as_dec);
			contents = getData(connfd);
			//getData(connfd);
		  //printf("%s\n", contents );

			memcpy(buf + strlen(buf), contents, size_as_dec);
			//printf("%s\n", buf);
			result = try_put(buf, MAXLINE - nremain);
			sleep(1);
			sendData(result, connfd);
			//sleep(1);


		}
	}
}

	//sendData(result, strlen(result), connfd);







/*
 * main() - parse command line, create a socket, handle requests
 */
int main(int argc, char **argv)
{
	/* for getopt */
	long opt;
	int  lru_size = 10;
	int  port     = 9000;
	bool multithread = false;

	check_team(argv[0]);

	/* parse the command-line options.  They are 'p' for port number,  */
	/* and 'l' for lru cache size, 'm' for multi-threaded.  'h' is also supported. */
	while((opt = getopt(argc, argv, "hml:p:")) != -1)
	{
		switch(opt)
		{
		case 'h': help(argv[0]); break;
		case 'l': lru_size = atoi(argv[0]); break;
		case 'm': multithread = true;	break;
		case 'p': port = atoi(optarg); break;
		}
	}

	/* open a socket, and start handling requests */
	int fd = open_server_socket(port);
	handle_requests(fd, file_server, lru_size, multithread);

	exit(0);
}
