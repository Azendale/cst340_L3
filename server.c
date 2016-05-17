// Written 2016-03 by Erik Andersen
// Modified 2016-04 by Erik Andersen
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
#include <signal.h>
#include <stdio.h>

#include "list.h"
#define BUFFSIZE 256

typedef struct 
{
    linked_list_t connections;
    int clientFd;
} thread_data_t;

typedef struct thisstruct
{
    pthread_t threadId;
    struct thisstruct * next;
    int clientFd;
} thread_list_node;

// Only written by main thread, and only set by main thread independent of previous value
// Read by other threads: if they see it as true, they attempt to shutdown at the next chance
bool serverShutdown;
// Only used by main thread. Needs to be global so signal handler can tell it to quit listening
int sockfd = -1;

void handleSIGINT(int sig)
{
    // Set the stop signal
    serverShutdown = true;
    shutdown(sockfd, SHUT_RD);
}

void shutConnection(int fd, void * userdata)
{
    shutdown(fd, SHUT_RD);
}

int fdRemoveCompare(int data, void * userData)
{
    int valueToRemove = *(int *)userData;
    
    if (valueToRemove == data)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

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

typedef struct
{
    char * messageBuf;
    int messageBufUsed;
} write_message_data;

void writeMessage(int outFd, void * userData)
{
    int messageBufUsed = ((write_message_data *)userData)->messageBufUsed;
    char* messageBuf = ((write_message_data *)userData)->messageBuf;
    int messageBufWritten = 0;
    int writtenThisRound = 0;
    while (messageBufWritten < messageBufUsed &&
        0 < (writtenThisRound = write(outFd, messageBuf, messageBufUsed-messageBufWritten))
    )
    {
        messageBufWritten += writtenThisRound;
    }
    if (writtenThisRound < 0)
    {
        fprintf(stderr, "Error writing to fd %d.\n", outFd);
    }
}

// Serve a new connection. void * arg is actually a file descriptor for the new
// connection
void * ThreadServeConnection(void * arg)
{
    // Needs to have all fds, as well as ours specifically
    thread_data_t * threadData = (thread_data_t *)arg;
    
    // Client we read from
    int clientSocket = threadData->clientFd;
    
    // All clients list, which we will broadcast message to.
    linked_list_t connections = threadData->connections;
    
    // Add our connection to the list of connections to send messages
    Insert_At_Beginning(connections, clientSocket);
    
    // Our buffer for copying
    char copyBuffer[BUFFSIZE];
    int copyBufferUsed = 0;
    
    // while there is still stuff to read from the client and the server isn't trying to shut down
    // try to read buffersize and set buffer used based on result
    while (!serverShutdown &&(0 < (copyBufferUsed = read(clientSocket, copyBuffer, BUFFSIZE))))
    {
        write_message_data writeInfo;
        writeInfo.messageBuf = copyBuffer;
        writeInfo.messageBufUsed = copyBufferUsed;
        // Attempt to write that buffer to each connection:
        if (0 != Traverse(connections, writeMessage, &writeInfo))
        {
            fprintf(stderr, "Error while trying to traverse connections list to write message from thread %ld", pthread_self());
        }
    }
    if (copyBufferUsed < 0)
    {
        fprintf(stderr, "Error while trying to read from client socket fd %d, thread %ld, closing that connection.\n", clientSocket, pthread_self());
    }
    
    // Remove the fd from the list
    if (1 != DeleteItemsFilter(connections, fdRemoveCompare, &(threadData->clientFd)))
    {
        fprintf(stderr, "Warning, thread %ld did not remove 1 item from connections list when it tried to remove fd %d.\n", pthread_self(), threadData->clientFd);
    }
    
    // Close the fd
    close(threadData->clientFd);
    
    free(threadData);
    return NULL;
}

int main(int argc, char ** argv)
{
    printf("Server starting, version %s\n", GIT_VERSION);
    serverShutdown = false;
    char * portString = getPortString(argc, argv);
    
    linked_list_t connections = Init_List();
    if (NULL == connections)
    {
        fprintf(stderr, "Trouble creating clients tracking list.\n");
        exit(3);
    }
    
    thread_list_node * threads = NULL;
    
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
    
    // Set a signal handler so the server can be stopped with Ctrl-C
    signal(SIGINT, handleSIGINT);
    // Now we are set up to take connections. Start a thread for each.
    
    while (!serverShutdown)
    {
    int acceptfd = -1; 
        if (-1 == (acceptfd = accept(sockfd, NULL, NULL)))
        {
            if (EINVAL == errno)
            {
                printf("Sever interrupted while waiting to accept a connection. Shutting down.\n");
            }
            else
            {
                perror("Trouble accept()ing a connection");
            }
            // Things the man page says we should check for and try again after
            if (!(EAGAIN == errno || ENETDOWN == errno || EPROTO == errno || \
                ENOPROTOOPT == errno || EHOSTDOWN == errno || ENONET == errno \
                || EHOSTUNREACH == errno || EOPNOTSUPP == errno || ENETUNREACH))
            {
                // Something the man page didn't list went wrong, let's give up
                serverShutdown = true;
            }
        }
        else
        {
            thread_data_t * threadData = (thread_data_t *)malloc(sizeof(thread_data_t));
            thread_list_node * thisThread = (thread_list_node *)malloc(sizeof(thread_list_node));
            if (NULL != threadData && NULL != thisThread)
            {
                thisThread->next = threads;
                threadData->clientFd = acceptfd;
                thisThread->clientFd = acceptfd;
                threadData->connections = connections;
                pthread_create(&(thisThread->threadId), NULL, ThreadServeConnection, threadData);
                // Now we have a valid thread, add it to the list of ones we'll wait for
                threads = thisThread;
            }
            else if (threadData)
            {
                free(threadData);
            }
            else // One of them was NULL, and it wasn't threadData, so it must be thisThread
            {
                free(thisThread);
            }
        }
    }
    
    Traverse(connections, shutConnection, NULL);
    
    // Now in shutdown mode
    // Clean up thread data
    while (threads)
    {
        thread_list_node * thisthread = threads;
        threads = threads->next;
        if (pthread_join(thisthread->threadId, NULL) != 0)
        {
            fprintf(stderr, "Got an error while trying to join thread %ld.\n", thisthread->threadId);
        }
        free(thisthread);
    }
    
    free(connections);
    // pthread_exit(0);
    return 0;
}