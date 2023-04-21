#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#define BUFFER_SIZE 2048

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[])
{
    int sockfd, newsockfd, portno, n;
    char buffer[255];

    if(argv < 2)
    {
        printf("No Port Provided\n");
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);
    char* ip = "127.0.0.1";

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0)
    {
        error("Error Opening Socket\n");
    }

    server = gethostbyname(argv[1]);

    if(server == NULL)
    {
        error("Error, no such host\n");
    }

    //setting server configuration
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr , (char *) &server_addr.sin_addr.s_addr , server->h_length);
    server_addr.sin_port = htons(portno);

    if(connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        error("Connect Failed.");
    }
    
    while(1)
    {   
        bzero(buffer , BUFFER_SIZE);
        fgets(buffer , BUFFER_SIZE, stdin);
        
        n = write(sockfd, buffer, strlen(buffer));
        
        if(n < 0)
        {
            error("Error on Writting.");
        }
        bzero(buffer , 255);
        n = read(sockfd, buffer, 255);
        if(n < 0)
        {
            error("Error on Reading.");
        }
        printf("Server : %s\n",buffer);
        if(strncmp("Bye" , buffer ,3) == 0)
        {
            break;
        }
    }

}