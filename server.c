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

#define MAX_CLIENTS 20
#define BUFFER_SIZE 2048
#define NAME_LENGTH 32
#define MAX_CLIENTS_CHANNEL 10

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
static char* pass = "mlsc2023";

// //Channel Structure
// typedef struct
// {
//     int client_uid[MAX_CLIENTS_CHANNEL];

// };


//Client Structure
typedef struct{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LENGTH];
    int admin;
    int leave_flag;
    int ignored_by_uids[MAX_CLIENTS];
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

void initialize_ignore(client_t* cli)
{
    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        cli->ignored_by_uids[i] = 0;
    }
}



void check()
{
    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        printf("%p\n", clients[i]);
    }
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
                clients[i] = NULL;
                break;                 
            }
        }
    }
    // check();
    pthread_mutex_unlock(&clients_mutex);
}

int ignore_check(client_t* cli1, client_t* cli2)
{
    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(cli1->ignored_by_uids[i] == cli2->uid)
        {
            return 1;
        }
    }
    return 0;
}

void send_message(char* s, client_t *cli)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(clients[i])
        {
            if(clients[i]->uid != cli->uid && !(ignore_check(cli, clients[i])))
            {
                if(write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("ERROR : write to descriptor\n");
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

char* extract(char* msg)
{
    char* wrd = malloc(strlen(msg));
    for(int i = 0; i< strlen(msg); i++)
    {
        wrd[i] = msg[i];
        if(msg[i] == ' ')
        {
            wrd[i] = '\0';
            break;
        }
    }
    return wrd;
}

client_t* search_client_by_name(char *name)
{
    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(clients[i])
        {
            if(strcmp(clients[i]->name, name) == 0)
            {
                return clients[i];
            }
        }
    }
    return NULL;
}

void send_message_everyone(char* s)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(clients[i])
        {
            if(write(clients[i]->sockfd, s, strlen(s)) < 0)
            {
                perror("ERROR : write to descriptor\n");
                break;
            }
            
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void print_and_send_evryone(char* msg)
{
    printf("%s", msg);
    send_message_everyone(msg);
}

void kick_person(char *name)
{
    char *msg = malloc(strlen(name)+20);
    client_t* cli = search_client_by_name(name);
    if(cli)
    {
        if(cli->admin == 1)
        {
            sprintf(msg, "Cannot Kick an admin\n");
            print_and_send_evryone(msg);
            return;
        }
        if(strcmp(cli->name, name) == 0)
        {
            sprintf(msg, "%s has been kicked\n", cli->name);
            print_and_send_evryone(msg);
            cli->leave_flag = 1;
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        print_and_send_evryone(msg);
    }
    free(msg);
    return;
}

void remove_admin(char *name)
{
    char *msg = malloc(strlen(name)+20);
    client_t* cli = search_client_by_name(name);
    if(cli)
    {
        if(strcmp(cli->name, name) == 0)
        {
            sprintf(msg, "%s is not an admin anymore\n", cli->name);
            print_and_send_evryone(msg);
            cli->admin = 0;
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        print_and_send_evryone(msg);
    }
    free(msg);
    return;
}

void make_admin(char *name)
{
    char *msg = malloc(strlen(name)+20);
    client_t* cli = search_client_by_name(name);
    if(cli)
    {
        if(strcmp(cli->name, name) == 0)
        {
            sprintf(msg, "%s is now an admin\n", cli->name);
            print_and_send_evryone(msg);
            cli->admin = 1;
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        print_and_send_evryone(msg);
    }
    free(msg);
    return;
}

void change_name(client_t* cli, char *name)
{
    char msg[strlen(name) + 20];
    sprintf(msg, "%s's name changed to %s\n", cli->name, name);
    print_and_send_evryone(msg);
    bcopy(name, cli->name, strlen(name));
}

void ignore_client(char* name, client_t *cli)
{
    int flag = 0;
    client_t* ignored_cli = search_client_by_name(name);
    if(ignored_cli)
    {
        for(int i = 0; i<MAX_CLIENTS; i++)
        {
            if(ignored_cli->ignored_by_uids[i] == 0)
            {
                ignored_cli->ignored_by_uids[i] = cli->uid;
                flag = 1; 
                break;
            }
        }
        if(flag)
        {
            char msg[strlen(ignored_cli->name) + 30];
            sprintf(msg, "Ignoring %s's Messages\n", ignored_cli->name);
            send(cli->sockfd, msg, strlen(msg), 0);
        }
    }
    else
    {
        send(cli->sockfd, "Name not Found\n", strlen("Name not Found\n"), 0);
    }
}

void unmute_client(char* name, client_t *cli)
{
    int flag = 0;
    client_t* to_be_unmted_cli = search_client_by_name(name);
    if(to_be_unmted_cli)
    {
        for(int i = 0; i<MAX_CLIENTS; i++)
        {
            if(to_be_unmted_cli->ignored_by_uids[i] == cli->uid)
            {
                to_be_unmted_cli->ignored_by_uids[i] = 0;
                flag = 1; 
                break;
            }
        }
        if(flag)
        {
            char msg[strlen(to_be_unmted_cli->name) + 30];
            sprintf(msg, "No Longer ignoring %s's Messages\n", to_be_unmted_cli->name);
            send(cli->sockfd, msg, strlen(msg), 0);
        }
    }
    else
    {
        send(cli->sockfd, "Name not Found\n", strlen("Name not Found\n"), 0);
    }
}

void handle_commands(char *cmd, client_t *cli)
{
    char *msg = extract(cmd);
    if(strcmp(msg, "/kick") == 0)
    {
        free(msg);
        if(cli->admin)
        {
            kick_person(cmd+6);
        }
        else
        {
            print_and_send_evryone("Admin Rights Required\n");
        }
    }
    else if(strcmp(msg, "/admin") == 0)
    {
        free(msg);
        if(cli->admin)
        {
            make_admin(cmd+7);
        }
        else
        {
            print_and_send_evryone("Admin Rights Required\n");
        }
    }
    else if(strcmp(msg, "/removeadmin")== 0)
    {
        free(msg);
        if(cli->admin)
        {
            remove_admin(cmd+13);
        }
        else
        {
            print_and_send_evryone("Admin Rights Required\n");
        }
    }
    else if(strcmp(msg, "/name") == 0)
    {
        free(msg);
        change_name(cli, cmd+6);
    }
    else if(strcmp(msg, "/mute") == 0)
    {
        free(msg);
        ignore_client(cmd+6, cli);
    }
    else if(strcmp(msg, "/unmute") == 0)
    {
        free(msg);
        unmute_client(cmd+8, cli);
    }
    else{
        free(msg);
        print_and_send_evryone("Wrong Command\n");
    }
}

void handle_client(void *arg)
{
    char buffer[BUFFER_SIZE];
    char name[NAME_LENGTH];
    

    cli_count++;

    client_t *cli = (client_t*) arg;
    printf("connected\n");
    cli->leave_flag = 0;
    
    //setting up ignored_int array
    initialize_ignore(cli);


    //setting up name 
   fflush(stdin);
    if( recv(cli->sockfd, name, NAME_LENGTH, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LENGTH-1)
    {
        perror("Rec failed");
        printf("Enter the Name Correctly\n");
        cli->leave_flag = 1;
    }
    else{
        strcpy(cli->name, name);
        str_trim_lf(cli->name, strlen(cli->name));
        bzero(buffer, strlen(buffer));
        sprintf(buffer, "%s has joined\n", cli->name);
        printf("%s", buffer);
        send_message(buffer, cli);
    }
    bzero(buffer, BUFFER_SIZE);

    //setting up admin rights initially
        fflush(stdin);
        char pass_rec[20];
        if( recv(cli->sockfd, pass_rec, 20, 0) <= 0)
        {
            perror("Rec failed");
            cli->leave_flag = 1;
        }
        else{
            str_trim_lf(pass_rec, strlen(pass_rec));
            printf("%s\n", pass_rec);
            printf("%s\n", pass);
            if(strcmp(pass, pass_rec) == 0)
            {
                make_admin(cli->name);
            }
            else
            {
                char* msg = "Wrong Password\n";
                send(cli->sockfd, msg,strlen(msg),0);
            }
        }
    
    while(1)
    {
        if(cli->leave_flag)
        {
            break;
        }
        int recieve = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if(strcmp(buffer,"/leave") == 0)
        {
            sprintf(buffer, "%s has left\n", cli->name);
            printf("%s", buffer);
            send_message(buffer, cli);
            cli->leave_flag = 1;
        }
        else if(recieve > 0)
        {
            if(strlen(buffer) > 0)
            {
                if(cli->leave_flag)
                {
                    break;
                }
                printf("%s : %s\n", cli->name,buffer);
                char msg[strlen(buffer) + strlen(cli->name) + 3];
                sprintf(msg, "%s : %s", cli->name, buffer);
                send_message(msg, cli);
                
                //commands section

                if( buffer[0] -'/' == 0)
                {
                    handle_commands(buffer, cli);
                }
                bzero(buffer, strlen(buffer));
            }
        }
        else 
        {
            printf("ERROR : -1\n");
            cli->leave_flag = 1;
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
            perror("Errors\n");
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