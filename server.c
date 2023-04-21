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

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048
#define NAME_LENGTH 32

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

//Client Structure
typedef struct{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LENGTH];
}client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout()
{
    printf("\r%s" , "> ");
    fflush(stdout);
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

void queue_add(client_t* cl)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }   

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(clients[i])
        {
            if(clients[i]->uid == uid)
            {
                clients[i] == NULL;
                break;                 
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char* s, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(clients[i])
        {
            if(clients[i]->uid != uid)
            {
                if(write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    printf("ERROR : write to descriptor\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void print_ip_addr(struct sockaddr_in addr)
{
    printf("Ip Address : %d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, 
                                       (addr.sin_addr.s_addr & 0xff0) >> 8,
                                       (addr.sin_addr.s_addr & 0xff0000) >> 16,
                                       (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void error(int check, char* msg)
{
    if(check < 0)
        perror(msg);
}

void handle_client(void *arg)
{
    char buffer[BUFFER_SIZE];
    char name[NAME_LENGTH];
    int leave_flag = 0;

    cli_count++;

    client_t *cli = (client_t*) arg;
    printf("connected\n");
    
    //name
   fflush(stdin);
    if( recv(cli->sockfd, name, NAME_LENGTH, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LENGTH-1)
    {
        perror("Rec failed");
        printf("Enter the Name Correctly\n");
        leave_flag = 1;
    }
    else{
        strcpy(cli->name, name);
        str_trim_lf(cli->name, strlen(cli->name));
        bzero(buffer, strlen(buffer));
        sprintf(buffer, "%s has joined\n", cli->name);
        printf("%s", buffer);
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER_SIZE);

    while(1)
    {
        if(leave_flag)
        {
            break;
        }
        bzero(buffer, BUFFER_SIZE);
        //sprintf(buffer, "%s : ", cli->name);
        error(write(cli->sockfd, buffer, strlen(buffer)), "Error on Write\n");
        bzero(buffer, BUFFER_SIZE);
        int recieve = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        
        if(strcmp(buffer,"exit") == 13)
        {
            sprintf(buffer, "%s has left\n", cli->name);
            printf("%s", buffer);
            send_message(buffer, cli->uid);
            leave_flag = 1;
        }
        else if(recieve > 0)
        {
            if(strlen(buffer) > 0)
            {
                send_message(buffer, cli->uid);
                printf("%s", buffer);
                bzero(buffer, strlen(buffer));
            }
        }
        else 
        {
            printf("ERROR : -1\n");
            leave_flag = 1;
        }
        bzero(buffer, BUFFER_SIZE);
    }
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return;
}

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("No Port Number Provided\n");
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    char* ip = "127.0.0.1";
    int port = atoi(argv[1]);
    
    int option = 1;
    int listenFd = 0, connectFd = 0;
    struct sockaddr_in server_addr, client_addr;

    pthread_t thread;

    listenFd = socket(AF_INET , SOCK_STREAM, 0);
    error(listenFd, "Error on socket creation\n");

    //Server Config
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //Signals
    signal(SIGPIPE, SIG_IGN);


    //setting up the Server
    error(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)), "ERROR: setsockopt\n");

    error(bind(listenFd, (struct sockaddr*)&server_addr, sizeof(server_addr)), "ERROR : Bind Error\n");

    error(listen(listenFd, 5), "ERROR : Error on Listen\n");

    printf("======== WELCOME TO THE CHATROOM ========\n");

    //communication
    while(1)
    {
        socklen_t clilen = sizeof(client_addr);
        connectFd = accept(listenFd, (struct sockaddr*)&client_addr, &clilen);

        if(connectFd < 0)
        {
            perror("Error\n");
            exit(0);
        }


        //check for max clients
        if((cli_count + 1) == MAX_CLIENTS)
        {
            printf("Maximum Clients connected. Connection Rejected.\n");
            print_ip_addr(client_addr);
            close(connectFd);
            continue;
        }

        //client settings
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->sockfd = connectFd;
        cli->uid = uid++;

        //Add client to queue

        queue_add(cli);
        pthread_create(&thread, NULL, (void *)&handle_client, (void*)cli);

        //reduce CPU usage
        sleep(1);
    }

    return EXIT_SUCCESS;
}


