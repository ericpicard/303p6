#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
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
#include "Client.h"
void sendData(void * buf, size_t n, int fd);
void help(char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("Perform a PUT or a GET from a network file server\n");
	printf("  -P    PUT file indicated by parameter\n");
	printf("  -G    GET file indicated by parameter\n");
	printf("  -s    server info (IP or hostname)\n");
	printf("  -p    port on which to contact server\n");
	printf("  -S    for GETs, name to use when saving file locally\n");
}

void die(const char *msg1, const char *msg2)
{
	fprintf(stderr, "%s, %s\n", msg1, msg2);
	exit(0);
}



void sendData(int fd,char * strbuf, size_t size){
  size_t n = size;
  size_t nremain = n;
  ssize_t nsofar;
  char *strbufp = strbuf;
  // printf("sending: %s",strbuf);
  while (nremain > 0) {
    if ((nsofar = write(fd, strbufp, nremain)) <= 0) {
      if (errno != EINTR) {

        fprintf(stderr, "Write error: %s\n", strerror(errno));
        exit(0);
      }
      nsofar = 0;
    }
    nremain -= nsofar;
    strbufp += nsofar;
  }
}
/*
 * connect_to_server() - open a connection to the server specified by the
 *                       parameters
 */
int connect_to_server(char *server, int port)
{
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;
	char errbuf[256];                                   /* for errors */

	/* create a socket */
	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		die("Error creating socket: ", strerror(errno));
	}

	/* Fill in the server's IP address and port */
	if((hp = gethostbyname(server)) == NULL)
	{
		sprintf(errbuf, "%d", h_errno);
		die("DNS error: DNS error ", errbuf);
	}
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	/* connect */
	if(connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		die("Error connecting: ", strerror(errno));
	}
	return clientfd;
}

/*
 * echo_client() - this is dummy code to show how to read and write on a
 *                 socket when there can be short counts.  The code
 *                 implements an "echo" client.
 */
void echo_client(int fd)
{
	// main loop
	while(1)
	{
		/* set up a buffer, clear it, and read keyboard input */
		const int MAXLINE = 8192;
		char buf[MAXLINE];
		bzero(buf, MAXLINE);
		if(fgets(buf, MAXLINE, stdin) == NULL)
		{
			if(ferror(stdin))
			{
				die("fgets error", strerror(errno));
			}
			break;
		}

		/* send keystrokes to the server, handling short counts */
		size_t n = strlen(buf);
		size_t nremain = n;
		ssize_t nsofar;
		char *bufp = buf;
		while(nremain > 0)
		{
			if((nsofar = write(fd, bufp, nremain)) <= 0)
			{
				if(errno != EINTR)
				{
					fprintf(stderr, "Write error: %s\n", strerror(errno));
					exit(0);
				}
				nsofar = 0;
			}
			nremain -= nsofar;
			bufp += nsofar;
		}

		/* read input back from socket (again, handle short counts)*/
		bzero(buf, MAXLINE);
		bufp = buf;
		nremain = MAXLINE;
		while(1)
		{
			if((nsofar = read(fd, bufp, nremain)) < 0)
			{
				if(errno != EINTR)
				{
					die("read error: ", strerror(errno));
				}
				continue;
			}
			/* in echo, server should never EOF */
			if(nsofar == 0)
			{
				die("Server error: ", "received EOF");
			}
			bufp += nsofar;
			nremain -= nsofar;
			if(*(bufp-1) == '\n')
			{
				*bufp = 0;
				break;
			}
		}

		/* output the result */
		printf("%s", buf);
	}
}


void recvData(int connfd){
	const int MAXLINE = 8192;
	char      buf[MAXLINE];   /* a place to store text from the client */
	bzero(buf, MAXLINE);

	/* read from socket, recognizing that we may get short counts */
	char *bufp = buf;              /* current pointer into buffer */
	ssize_t nremain = MAXLINE;     /* max characters we can still read */
	size_t nsofar;                 /* characters read so far */
	while (1)
	{
		//printf("%s\n","shedding" );
		/* read some data; swallow EINTRs */
		if((nsofar = read(connfd, bufp, nremain)) < 0)
		{
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
			return;
		}
		/* update pointer for next bit of reading */
		bufp += nsofar;
		nremain -= nsofar;
	if(*(bufp-1) == '\n')
		{
			*bufp = 0;
			break;
		}
	}
	printf("%s\n", buf );

return;

}
/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *put_name){
	const int MAXLINE = 8192;
	char buf[MAXLINE];
	strcpy(buf, "PUT ");
	size_t n = strlen(buf);
	size_t nremain = n;
	ssize_t nsofar;
	char * bufp = buf;
	strcat(buf, put_name);
	strcat(buf, "\n");
	//sendData(fd, buf, strlen(buf));
	//bzero(buf, MAXLINE);

	if(put_name == NULL){
		fprintf(stderr, "%s\n", "Please provide a file name." );
		gets(put_name);
	}
	if(access(put_name, F_OK) != -1){
		///printf("%s", buf );
		sendData(fd, buf, strlen(buf));
		sleep(1);
		FILE *fileptr = fopen(put_name, "rb");
		fseek(fileptr, 0, SEEK_END);
    long filelen = ftell(fileptr);
    rewind(fileptr);
		char * contents;
		contents = (char *)malloc((filelen+1)*sizeof(char)); // Enough memory for file + \0
		fread(contents, filelen, 1, fileptr); // Read in the entire file
		fclose(fileptr); // Close the file
		//memcpy(contents + filelen, "\n", 1);
		char fsize[MAXLINE];
		sprintf(fsize, "%ld\n", filelen);
		sendData(fd, fsize, strlen(fsize));
    sleep(1);
		//memcpy(contents + filelen, buf, 1 );
		sendData(fd, contents, filelen);
	//printf("%s", contents);

		sleep(1);
		recvData(fd);
		exit(-1);

	} else {
	perror("Error: cannot put specified file. File does not exist.");
	exit(-1);
	}

}


/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(int fd, char *get_name, char *save_name)
{

}

/*
 * main() - parse command line, open a socket, transfer a file
 */
int main(int argc, char **argv)
{
	/* for getopt */
	long  opt;
	char *server = NULL;
	char *put_name = NULL;
	char *get_name = NULL;
	int   port;
	char *save_name = NULL;

	check_team(argv[0]);

	/* parse the command-line options. */
	while((opt = getopt(argc, argv, "hs:P:G:S:p:")) != -1)
	{
		switch(opt)
		{
		case 'h': help(argv[0]); break;
		case 's': server = optarg; break;
		case 'P': put_name = optarg; break;
		case 'G': get_name = optarg; break;
		case 'S': save_name = optarg; break;
		case 'p': port = atoi(optarg); break;
		}
	}

	/* open a connection to the server */
	int fd = connect_to_server(server, port);

	/* put or get, as appropriate */
	if(put_name)
	{
		put_file(fd, put_name);
	}
	else
	{
		get_file(fd, get_name, save_name);
	}

	/* close the socket */
	int rc;
	if((rc = close(fd)) < 0)
	{
		die("Close error: ", strerror(errno));
	}
	exit(0);
}
