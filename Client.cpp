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
#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>
#include "support.h"
#include "Client.h"
using namespace std;
void sendData(int fd,char * strbuf, size_t size);
void help(char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("Perform a PUT or a GET from a network file server\n");
	printf("  -P    PUT file indicated by parameter\n");
	printf("  -G    GET file indicated by parameter\n");
	printf("  -E    Use RSA encryption to protect files on server\n");
	printf("  -C    use checksums for both PUT and GET\n");
	printf("  -s    server info (IP or hostname)\n");
	printf("  -p    port on which to contact server\n");
	printf("  -S    for GETs, name to use when saving file locally\n");
}
int padding = RSA_PKCS1_PADDING;
RSA * createRSAWithFilename(char * filename,int is_public){
  FILE * fp = fopen(filename,"rb");
  if(fp == NULL){
    printf("Unable to open file %s \n",filename);
    return NULL;
  }
  RSA *rsa= RSA_new();
  if(is_public){
    rsa = PEM_read_RSA_PUBKEY(fp, &rsa,NULL, NULL);
  }
  else{
    rsa = PEM_read_RSAPrivateKey(fp, &rsa,NULL, NULL);
  }
  return rsa;
}
int public_encrypt(unsigned char * data,int data_len, char * key, unsigned char *encrypted)
{
  RSA * rsa = createRSAWithFilename(key,1);
  int result = RSA_public_encrypt(data_len,data,encrypted,rsa,padding);
  return result;
}
int private_decrypt(unsigned char * enc_data,int data_len, char * key, unsigned char *decrypted)
{
  RSA * rsa = createRSAWithFilename(key,0);
  int  result = RSA_private_decrypt(data_len,enc_data,decrypted,rsa,padding);
  return result;
}
void printLastError(char *msg){
  char * err = (char*) malloc(130);
  ERR_load_crypto_strings();
  ERR_error_string(ERR_get_error(), err);
  printf("%s ERROR: %s\n",msg, err);
  free(err);
}

void die(const char *msg1, const char *msg2)
{
	fprintf(stderr, "%s, %s\n", msg1, msg2);
	exit(0);
}


char *md5sum(const char *str, int length) {
  int n;
  MD5_CTX c;
  unsigned char digest[16];
  char *out = (char*)malloc(33);

  MD5_Init(&c);

  while (length > 0) {
    if (length > 512) {
      MD5_Update(&c, str, 512);
    } else {
      MD5_Update(&c, str, length);
    }
    length -= 512;
    str += 512;
  }

  MD5_Final(digest, &c);
  for (n = 0; n < 16; ++n) {
    snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
  }
  return out;
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


char* getData(int connfd){
	const int MAXLINE = 8192;
	char *      buf = (char*)malloc(sizeof(char) *MAXLINE);   /* a place to store text from the client */
	//bzero(buf, MAXLINE);

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
			return buf;
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


	//printf("%s\n", buf );

return buf;

}
/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *put_name, int checksums){
	const int MAXLINE = 8192;
	char buf[MAXLINE];
	char* checksum = (char *)malloc(sizeof(char)*MAXLINE);
	char* response = (char *)malloc(sizeof(char)*MAXLINE);
	if(!checksums){
	strcpy(buf, "PUT ");
}
else{
	strcpy(buf, "PUTC ");
}
if(put_name == NULL){
	fprintf(stderr, "%s\n", "Please provide a file name." );
	gets(put_name);
}
	size_t n = strlen(buf);
	size_t nremain = n;
	ssize_t nsofar;
	char * bufp = buf;
	strcat(buf, put_name);
	strcat(buf, "\n");
	//sendData(fd, buf, strlen(buf));
	//bzero(buf, MAXLINE);


	if(access(put_name, F_OK) != -1){
		///printf("%s", buf );
		sendData(fd, buf, strlen(buf));
		sleep(1);
		FILE *fileptr = fopen(put_name, "rb");
		fseek(fileptr, 0, SEEK_END);
    long filelen = ftell(fileptr);
    rewind(fileptr);
		char * contents;
		char * encryptedContents;
		unsigned char* contentsforRSA = (unsigned char *)malloc((filelen+1)*sizeof(char));
		contents = (char *)malloc((filelen+1)*sizeof(char));
		encryptedContents = (char *)malloc((filelen+1)*sizeof(char));// Enough memory for file + \0
		fread(contents, filelen, 1, fileptr); // Read in the entire file
		fclose(fileptr); // Close the file
		//memcpy(contents + filelen, "\n", 1);
		unsigned char  encrypted[4098]={};
    unsigned char decrypted[4098]={};
		char pubkey[MAXLINE];
		strcpy(pubkey, "public.pem");
		memcpy(contentsforRSA, contents, filelen);
    int encrypted_length= public_encrypt(contentsforRSA,filelen,pubkey,encrypted);
		char fsize[MAXLINE];

		sprintf(fsize, "%ld\n", filelen);
		printf("%s\n", encrypted );
		sendData(fd, fsize, strlen(fsize));
    sleep(1);
		if(checksums){
			checksum = md5sum(contents, filelen);
			printf("%s\n",checksum);

			strcat(checksum, "\n");
			sendData(fd,checksum, 33);
			sleep(1);
		}
		//memcpy(contents + filelen, buf, 1 );
		sendData(fd, contents, filelen);
	//printf("%s", contents);

		sleep(1);

		free(contents);
		response = getData(fd);
		printf("%s\n", response );
		//exit(-1);

	} else {
	perror("Error: cannot put specified file. File does not exist.");
	exit(-1);
	}

}


/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
 void get_file(int fd, char *get_name, char *save_name, int checksums)
 {
	 int allow_write = 1;
printf("%d\n", checksums );
 		/* TODO: implement a proper solution, instead of calling the echo() client */
 		//echo_client(fd);
 		const int MAXLINE = 8192;
 		char request[MAXLINE];
		char* response = (char*)malloc(sizeof(char)* MAXLINE);
		char* bytes = (char*)malloc(sizeof(char)* MAXLINE);
		char* checksum = (char*)malloc(sizeof(char)* MAXLINE);
		char* contents = (char*)malloc(sizeof(char)* MAXLINE);
 		/* Send the get request to server*/
		if(checksums == 0){
 		sprintf(request,"GET %s\n", get_name);
	}
	else{
		sprintf(request,"GETC %s\n", get_name);
	}
 		printf("%s\n", get_name);
 		sendData(fd, request, strlen(request));
		printf("%s\n",request );
 		sleep(1);
 		response = getData(fd);
		printf("%s\n", response);
		if(strncmp(response, "OK", 2) == 0){

 		bytes = getData(fd);
		if(checksums){
			checksum = getData(fd);
			printf("%s\n", checksum);
		}
	//	sleep(1);
 		contents = getData(fd);
		printf("%s\n", bytes );
		printf("%s\n", contents );
		stringstream str(bytes);
		int x;
		str >> x;
		printf("%d\n", x);

if(checksums){
		printf("%s\n","Comparing checksum values." );
		printf("%s\n", md5sum(contents, x) );
		if(memcmp(checksum, md5sum(contents, x), 32 ) == 0){
			printf("%s\n", "Received valid checksum.");
		}else{
			printf("%s\n", "Error. Checksum mismatch.");
			allow_write = 0;
}
}
if(allow_write){
 		if(FILE *fp = fopen(get_name, "wb")){
		fwrite(contents, sizeof(char), (x), fp);
 		//fprintf(fp, "%s", reply);

 		if(save_name){

 		rename(get_name,save_name);
 	}
 		fclose(fp);
}
else printf("%s\n", "Error Opening File.");
}
}
 		/* Receive Response*/
 		//exit(0);

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
	int checksums = 0;

	int   port;
	char *save_name = NULL;

	check_team(argv[0]);

	/* parse the command-line options. */
	while((opt = getopt(argc, argv, "hs:P:G:S:p:C")) != -1)
	{
		switch(opt)
		{
		case 'h': help(argv[0]); break;
		case 's': server = optarg; break;
		case 'P': put_name = optarg; break;
		case 'G': get_name = optarg; break;
		case 'C': checksums = 1;     break;
		case 'S': save_name = optarg; break;
		case 'p': port = atoi(optarg); break;
		}
	}

	/* open a connection to the server */
	int fd = connect_to_server(server, port);

	/* put or get, as appropriate */

	if(put_name)
	{
		put_file(fd, put_name, checksums);
	}
	else
	{
		get_file(fd, get_name, save_name, checksums);
	}




	/* close the socket */
	int rc;
	if((rc = close(fd)) < 0)
	{
		die("Close error: ", strerror(errno));
	}
	exit(0);
}
