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
#define NAME_LENGTH 32

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LENGTH];

void str_overwrite_stdout()
{
    printf("\r%s" , "> ");
    fflush(stdout);
}


void catch_ctrl_c_and_exit()
{
    flag = 1;
}

void str_trim_lf(char* arr, int length)
{
    for(int i = 0; i<length; i++)
    {
        
        if(arr[i] == 13 || arr[i] == 10)
        {
            arr[i] = '\0';
            break;
        }
    }
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void send_msg_handler()
{
    char buffer[BUFFER_SIZE] = {};
    // msg[BUFFER_SIZE + NAME_LENGTH + 5] = {};
    
    while(1)
    {
        //str_overwrite_stdout();
        fgets(buffer, BUFFER_SIZE, stdin);
        str_trim_lf(buffer, BUFFER_SIZE);

        if(strcmp(buffer, "exit") == 0)
        {
            if(write(sockfd, buffer, strlen(buffer)) < 0)
            {
                perror("Unable to write\n");
            }
            break;
        }
        else
        {
            if(write(sockfd, buffer, strlen(buffer)) < 0)
            {
                perror("Unable to write\n");
            }
        }

        bzero(buffer, BUFFER_SIZE);
        //bzero(msg, BUFFER_SIZE+NAME_LENGTH +5);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler()
{
    char msg[BUFFER_SIZE] = {};
    while(1)
    {
        int recieve = recv(sockfd, msg, BUFFER_SIZE, 0);

        //str_trim_lf(msg, strlen(msg));

        if(recieve > 0)
        {
            printf("%s\n",msg);
            str_overwrite_stdout();
        }
        else if(recieve == 0)
        {
            break;
        }
        bzero(msg, BUFFER_SIZE);
    }
}

int main(int argc, char* argv[])
{
    int port;
    char buffer[BUFFER_SIZE];

    if(argc < 2)
    {
        printf("No Port Provided\n");
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, catch_ctrl_c_and_exit);

    port = atoi(argv[1]);
    char* ip = "127.0.0.1";

    //Socket settings
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        error("Error Opening Socket\n");
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //connecting to the server
    if(connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        error("Connect Failed.");
    }
    
    //getting name
    printf("Enter your Name : ");
    fgets(name, NAME_LENGTH, stdin);
    str_trim_lf(name, strlen(name));

    if(strlen(name) < 2 || strlen(name) > NAME_LENGTH -1 )
    {
        printf("Enter the Name Correctly\n");
        return EXIT_FAILURE;
    }

    send(sockfd, name, NAME_LENGTH, 0);
    printf("======== WELCOME TO THE CHATROOM ========\n");

    //creating threads
    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void*)send_msg_handler, NULL) != 0)
    {
        error("Error in creating threads.\n");
    }
    pthread_t recieve_msg_thread;
    if(pthread_create(&recieve_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0)
    {
        error("Error on Creating Threads\n");
    }

    while(1)
    {
        if(flag)
        {
            printf("\nBye\n");
            break;
        }

    }
    close(sockfd);

    return EXIT_SUCCESS;
}