// Written 2016-03 by Erik Andersen
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

#define BUFFER_SIZE 256

// Contains an easy to use representation of the command line args
typedef struct
{
    char * port;
    char * address;
	char * clientName;
} program_options;

// Set up our struct -- note that I expect this to point to argv memory,
// so no destructor needed
void Init_program_options(program_options * options)
{
    options->port = NULL;
    options->address = NULL;
	options->clientName = NULL;
}

typedef struct
{
	int socketFd;
} io_thread_data;

// Parse command line args into the already allocatated program_options struct
// 'options'
void parseOptions(int argc, char ** argv, program_options * options)
{
    int portNum = 0;
    int arg;
    while (-1 != (arg = getopt(argc, argv, "s:n:i:p:")))
    {
        if ('p' == arg)
        {
            portNum = atoi(optarg);
            if (portNum < 1 || portNum > 65535)
            {
                fprintf(stderr, "Invalid port number given.\n");
                exit(1);
            }
            options->port = optarg;
        }
        // Treat -s and -i the same since getaddrinfo can handle them both
        else if ('s' == arg || 'i' == arg)
        {
            options->address = optarg;
        }
        else if ('n' == arg)
		{
			options->clientName = optarg;
		}
    }
    if (NULL == (options->address))
    {
        fprintf(stderr, "You must set either a hostname or address with -s or -i.\n");
        exit(2);
    }
    if (NULL == (options->port))
    {
        fprintf(stderr, "You must set a port number with the -p option.\n");
        exit(4);
    }
    if (NULL == (options->clientName))
	{
		fprintf(stderr, "You must pick a chat client username with the -n option.\n");
		exit(3);
	}
}

bool continueLoop = true;

void clientSIGINT(int signal)
{
	continueLoop = false;
}

void * inputLoop(void * userdata)
{
	char sendBuffer[BUFFER_SIZE];
	
	memset(sendBuffer, 0, BUFFER_SIZE);
	while (continueLoop && NULL != fgets(sendBuffer, BUFFER_SIZE, stdin))
	{
		int sendBufUsed = strlen(sendBuffer);
		int sendBufWritten = 0;
		int writtenThisRound = 0;
		while (sendBufWritten < sendBufUsed &&
			0 < (writtenThisRound = write(sockfd, sendBuffer, sendBufUsed-sendBufWritten))
		)
		{
			sendBufWritten += writtenThisRound;
		}
		fprintf(stderr, "Error writing to fd %d.\n", sockfd);
		memset(sendBuffer, 0, BUFFER_SIZE);
	}
}

void * outputLoop(void * userdata)
{
	char recvBuffer[BUFFER_SIZE];
	int socketfd = (io_thread_data *)(userdata)->socketFd;
	
	memset(recvBuffer, 0, BUFFER_SIZE);
	while (continueLoop && 0 != read(recvBuffer, BUFFER_SIZE, socketfd))
	{
		int recvBufWritten = 0;
		int writtenThisRound = 0;
		while (recvBufWritten < recvBufUsed &&
			0 < (writtenThisRound = write(stdout, recvBuffer, recvBufUsed-recvBufWritten))
		)
		{
			recvBufWritten += writtenThisRound;
		}
		fprintf(stderr, "Error writing to stdout.\n");
		memset(recv, 0, BUFFER_SIZE);
	}
}

int main(int argc, char ** argv)
{
    // Program options
    program_options options;
    Init_program_options(&options);
    parseOptions(argc, argv, &options);
    
    
    // For critera for lookup
    struct addrinfo hints;
    struct addrinfo * destInfoResults;
    
    // Initialize the struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Use IPv4 or IPv6, we don't care
    hints.ai_socktype = SOCK_STREAM; // Use tcp
    
    // Do the lookup
    int returnStatus = -1;
    if (0 != (returnStatus = getaddrinfo(options.address, options.port, &hints, &destInfoResults)) )
    {
        fprintf(stderr, "Couldn't get address lookup info: %s\n", gai_strerror(returnStatus));
        exit(8);
    }
    
    int sockfd = -1;
    
    // Loop through results until one works
    bool connectSuccess = false;
    struct addrinfo * p = destInfoResults;
    for (; (!connectSuccess) && NULL != p; p = p->ai_next)
    {
        if (-1 != (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
        {
            if (-1 != connect(sockfd, p->ai_addr, p->ai_addrlen))
            {
                connectSuccess = true;
            }
            else
            {
                close(sockfd);
                perror("Trouble connecting: ");
            }
        }
        else
        {
            perror("Trouble getting a socket: ");
        }
    }
    
	signal(SIGINT, clientSIGINT);
	
	io_thread_data inThreadData;
	io_thread_data outThreadData;
	pthread_t inThread, outThread;
	
	inThreadData.socketFd = sockfd;
	outThreadData.socketFd = sockfd;
	
	pthread_create(&inThread, NULL, ThreadServeConnection, threadData);
	
	
	
    close(sockfd);
    
    return 0;
}