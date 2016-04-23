// Written 2016-03 by Erik Andersen
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

// Uses getopt style arguments to get the port number, as a cstring
// don't need to free returned pointer as it points to argv
char * getPortString(int argc, char ** argv)
{
    char * portString = NULL;
    int arg;
    while (-1 != (arg = getopt(argc, argv, "p:")))
    {
        if ('p' == arg)
        {
            portString = optarg;
        }
    }
    if (NULL == portString)
    {
        fprintf(stderr, "No port number or service name set. Please specify it with -p <port_number>.\n");
        exit(4);
    }
    return portString;
}

// Serve a new connection. void * arg is actually a file descriptor for the new
// connection
void * ThreadServeConnection(void * arg)
{
    int acceptFd = (int)((long)arg);
    
    // Start by reading
    // loop until quit command
    bool continueLoop = true;
    // Can't directly include a literal, because setting socket opts needs the 
    // address of
    int corkFlag;
    // File we read to or from
    int fileFD;
    // memory (so read/write can operate on it) that holds command flags
    uint16_t messageBuffer = 0;
    char * filename = NULL;
    struct stat trustedSt;
    bool bailOut = false;
    while (continueLoop)
    {
        memset(&trustedSt, 0, sizeof(trustedSt));
        bailOut = false;
        
        // Find out what the client wants
        if (sizeof(messageBuffer) != read(acceptFd, &messageBuffer, sizeof(messageBuffer)))
        {
            fprintf(stderr, "Couldn't read command flag client sent.\n");
            bailOut = true;
            continueLoop = false;
        }
        
        if (!bailOut)
        {
            messageBuffer = ntohs(messageBuffer);
            if (OFTP_C_QUIT == messageBuffer)
            {
                fprintf(stderr, "Client sent quit command, exiting thread.\n");
                continueLoop = false;
            }
            else if (OFTP_C_GET == messageBuffer)
            {
                // Get file name they want from the connection
                if (NULL == (filename = getFname(acceptFd)))
                {
                    fprintf(stderr, "Couldn't read filename the client wanted.\n");
                    bailOut = true;
                }
                
                // Open the file they want
                if (!bailOut && -1 == (fileFD = open(filename, O_RDONLY)))
                {
                    fprintf(stderr, "Couldn't open for reading the file the client requested.\n");
                    // Couldn't open file error
                    messageBuffer = OFTP_R_NOSUCHFILE;
                    messageBuffer = htons(messageBuffer);
                    if (sizeof(messageBuffer) != write(acceptFd, &messageBuffer, sizeof(messageBuffer)))
                    {
                        // Wow, even giving an error failed. Give up on the connection
                        fprintf(stderr, "Couldn't even send client error that their request won't work on the file the requested because we can't open it.\n");
                        continueLoop = false;
                    }
                    // Either way, give up on this command
                    bailOut = true;
                }
                if (!bailOut && -1 == fstat(fileFD, &trustedSt))
                {
                    fprintf(stderr, "Couldn't fstat the file the client requested.\n");
                    // Couldn't open file error
                    messageBuffer = OFTP_R_NOSUCHFILE;
                    messageBuffer = htons(messageBuffer);
                    if (sizeof(messageBuffer) != write(acceptFd, &messageBuffer, sizeof(messageBuffer)))
                    {
                        // Wow, even giving an error failed. Give up on the connection
                        fprintf(stderr, "Couldn't even send client error that their request won't work on the file the requested because we can't fstat it.\n");
                        continueLoop = false;
                    }
                    // Either way, give up on this command
                    bailOut = true;
                }
                
                if (!bailOut)
                {
                    // Cork -- we are going to send a response followed by bulk
                    // data -- tell the kernel to hold our packets for up to
                    // 200ms while we pack them to full size
                    corkFlag = 1;
                    // Corking is not mandatory, so just print an error if something goes wrong
                    if (setsockopt(acceptFd, IPPROTO_TCP, TCP_CORK, &corkFlag, sizeof(corkFlag)) == -1)
                    {
                        perror("setsockopt(TCP_CORK)");
                        fprintf(stderr, "Couldn't set cork flag on the socket for get request.\n");
                        bailOut = true;
                    }
                }
                    
                // Send reply OFTP_R_OKFILENEXT with size
                if (!bailOut)
                {
                    messageBuffer = htons(OFTP_R_OKFILENEXT);
                    if (sizeof(messageBuffer) != write(acceptFd, &messageBuffer, sizeof(messageBuffer)))
                    {
                        fprintf(stderr, "Couldn't send client the 'ok here comes the file you wanted message' in response to their get request.\n");
                        bailOut = true;
                    }
                }
                
                
                // Send the actual file
                if (!bailOut && SendAFile(acceptFd, fileFD, trustedSt.st_size))
                {
                    fprintf(stderr, "Couldn't send the whole file the client requested.\n");
                    bailOut = true;
                }
                
                // We are done actively using the connection for a while, let
                // don't make the kernel wait wait for us to fill the packets
                corkFlag = 0;
                if (setsockopt(acceptFd, IPPROTO_TCP, TCP_CORK, &corkFlag, sizeof(corkFlag)) == -1)
                {
                    perror("setsockopt(TCP_CORK)");
                }
                
                close(fileFD);
                free(filename);
                filename = NULL;
            }
            else if (OFTP_C_PUT == messageBuffer)
            {
                // Can't use sendfile, so:
                // Find out filename
                if (NULL == (filename = getFname(acceptFd)))
                {
                    fprintf(stderr, "Couldn't read the file name of the file the client is going to send.\n");
                    bailOut = true;
                }
                
                // open dest file
                if(!bailOut && -1 == (fileFD = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)))
                {
                    // Couldn't open file, send error to client, no write access
                    fprintf(stderr, "Couldn't open local file for storing what was sent by the client.\n");
                    bailOut = true;
                }
                
                // Actually recieve the file
                if (!bailOut && ReceiveAFile(acceptFd, fileFD))
                {
                    fprintf(stderr, "Couldn't recieve the file put by the client.\n");
                    bailOut = true;
                    continueLoop = false;
                }
                
                close(fileFD);
                free(filename);
                filename = NULL;
            }
            else
            {
                fprintf(stderr, "Command sent by the client not recognized: %d.\n", messageBuffer);
                // Unknown command
                continueLoop = false;
            }
        }
    }
    
    close(acceptFd);
    return NULL;
}

int main(int argc, char ** argv)
{
    char * portString = getPortString(argc, argv);
    
    // Gives getaddrinfo hints about the critera for the addresses it returns
    struct addrinfo hints;
    // Points to list of results from getaddrinfo
    struct addrinfo *serverinfo;
    
    // Erase the struct
    memset(&hints, 0, sizeof(hints));
    
    // Then set what we actually want:
    hints.ai_family = AF_INET6;
    // We want to do TCP
    hints.ai_socktype = SOCK_STREAM;
    // We want to listen (we are the server)
    hints.ai_flags = AI_PASSIVE;
    
    int status = 0;
    if (0 != (status = getaddrinfo(NULL, portString, &hints, &serverinfo)))
    {
        // There was a problem, print it out
        fprintf(stderr, "Trouble with getaddrinfo, error was %s.\n", gai_strerror(status));
    }
    
    int sockfd = -1;
    struct addrinfo * current = serverinfo;
    // Traverse results list until one of them works to open
    sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    while (-1 == sockfd && NULL != current->ai_next)
    {
        current = current->ai_next;
        sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    }
    if (-1 == sockfd)
    {
        fprintf(stderr, "We tried valliantly, but we were unable to open the socket with what getaddrinfo gave us.\n");
        freeaddrinfo(serverinfo);
        exit(8);
    }
    
    // Ok, say we want a socket that abstracts away whether we are doing IPv6 or IPv4 by just having it handle IPv4 mapped addresses for us
    int no = 0;
    if (0 > setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)))
    {
        fprintf(stderr, "Trouble setting socket option to also listen on IPv4 in addition to IPv6. Falling back to IPv6 only.\n");
    }
    
    int yes = 1;
    if (0 > setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes)))
    {
        fprintf(stderr, "Couldn't set option to re-use addresses. The server will still try to start, but if the address & port has been in use recently (think last minute range), binding may fail.");
    }
    
    // Ok, so now we have a socket. Lets try to bind to it
    if (-1 == bind(sockfd, current->ai_addr, current->ai_addrlen))
    {
        // Couldn't bind
        fprintf(stderr, "We couldn't bind to the socket. (Or something like that. What is the correct terminology?)\n");
        exit(16);
    }
    
    // Also cleans up the memory pointed to by current
    freeaddrinfo(serverinfo);
    serverinfo = NULL;
    
    if (-1 == listen(sockfd, 10))
    {
        // Couldn't listen
        fprintf(stderr, "Call to listen failed.\n");
        exit(32);
    }
    
    // Now we are set up to take connections. Start a thread for each.
    
    
    // This part seems scary. I have a loop that never ends that allocates memory. Nothing could go wrong, right?
    bool continueLoop = true;
    pthread_t tid;
    while (continueLoop)
    {
       int acceptfd = -1; 
        if (-1 == (acceptfd = accept(sockfd, NULL, NULL)))
        {
            fprintf(stderr, "Trouble accept()ing a connection.\n");
            // Things the man page says we should check for and try again after
            if (!(EAGAIN == errno || ENETDOWN == errno || EPROTO == errno || \
                ENOPROTOOPT == errno || EHOSTDOWN == errno || ENONET == errno \
                || EHOSTUNREACH == errno || EOPNOTSUPP == errno || ENETUNREACH))
            {
                // Something the man page didn't list went wrong, let's give up
                continueLoop = false;
            }
        }
        else
        {
            pthread_create(&tid, NULL, ThreadServeConnection, (void *)(long)acceptfd);
            // Set detach because we aren't going to track these threads, and we
            // want associated memory freed when they quit
            pthread_detach(tid);
        }
    }
        
    pthread_exit(0);
    return 0;
}