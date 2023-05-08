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
#define MAX_CHANNELS 5

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
static char* pass = "mlsc2023";

//Client Structure
typedef struct{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LENGTH];
    int admin;
    int leave_flag;
    int ignored_by_uids[MAX_CLIENTS];
    int current_channel_uid;
    char chn_name[NAME_LENGTH];
}client_t;

//Channel Structure
typedef struct
{
    client_t *clients[MAX_CLIENTS_CHANNEL];
    int channel_uid;
    char name[NAME_LENGTH];
}channel_t;

channel_t *channels[MAX_CHANNELS];

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
    if(cli1 == cli2)
    {
        return 0;
    }
    for(int i = 0; i<MAX_CLIENTS; i++)
    {
        if(cli1->ignored_by_uids[i] == cli2->uid)
        {
            return 1;
        }
    }
    return 0;
}

channel_t* search_channel_by_uid(int uid)
{
    for(int i = 0; i<MAX_CHANNELS; i++)
    {
        if(channels[i])
        {
            if(channels[i]->channel_uid == uid)
            {
                return channels[i];
            }
        }
    }
    return NULL;
}

void send_message(char* s, client_t *cli)
{
    pthread_mutex_lock(&clients_mutex);

    channel_t* chn = search_channel_by_uid(cli->current_channel_uid);
    if(chn)
    {
        //printf("%s\n", chn->name);
        for(int i = 0; i<MAX_CLIENTS_CHANNEL; i++)
        {
            if(chn->clients[i])
            {
                if(chn->clients[i]->uid != cli->uid && !(ignore_check(cli, chn->clients[i])))
                {
                    printf("%d\n", chn->clients[i]->uid);
                    if(write(chn->clients[i]->sockfd, s, strlen(s)) < 0)
                    {
                        perror("ERROR : write to descriptor\n");
                        break;
                    }
                }
            }
        }
    }
    else
    {
        for(int i = 0 ;i<MAX_CLIENTS; i++)
        {
            if(clients[i])
            {
                if(clients[i]->current_channel_uid == 0 && clients[i]->uid != cli->uid)
                {
                    send(clients[i]->sockfd, s, strlen(s), 0);
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


client_t* search_client_by_name_in_channel(char *name, client_t* cli)
{
    channel_t* channel = search_channel_by_uid(cli->current_channel_uid);
    if(cli->current_channel_uid == 0)
    {
        for(int i = 0; i<MAX_CLIENTS; i++)
        {
            if(clients[i])
            {
                if(clients[i]->current_channel_uid == 0)
                {
                    if(strcmp(clients[i]->name, name) == 0)
                    {
                        return clients[i];
                    }
                }
            }
        }
    }
    else if(channel)
    {
        for(int i = 0; i<MAX_CLIENTS_CHANNEL; i++)
        {
            if(channel->clients[i])
            {
                if(strcmp(channel->clients[i]->name, name) == 0)
                {
                    return channel->clients[i];
                }
            }
        }
    }
    else 
    {
        printf("Error 406\n");
    }
    return NULL;
}

void send_message_everyone_in_channel(char* s, client_t* cli, int print)
{
    pthread_mutex_lock(&clients_mutex);

    channel_t* chn = search_channel_by_uid(cli->current_channel_uid);

    if(chn)
    {
        if(print)
        {
            printf("%s->%s\n",chn->name, s);
        }

        for(int i = 0; i<MAX_CLIENTS_CHANNEL; i++)
        {
            if(chn->clients[i])
            {
                if(write(chn->clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("ERROR : write to descriptor\n");
                    break;
                }
            }
        }
    }
    else
    {
        for(int i = 0 ;i<MAX_CLIENTS; i++)
        {
            if(clients[i])
            {
                if(clients[i]->current_channel_uid == 0)
                {
                    send(clients[i]->sockfd, s, strlen(s), 0);
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void check_channels()
{
    for(int i = 0; i<MAX_CHANNELS; i++)
    {
        printf("%p\n", channels[i]);
        if(channels[i])
            printf("%s\n", channels[i]->name);
        else 
            printf("NONE\n");
    }
}

void kick_person(char *name, client_t* cli)
{
    char *msg = malloc(strlen(name)+20);
    client_t* kicked_cli = search_client_by_name_in_channel(name, cli);
    if(kicked_cli)
    {
        if(kicked_cli->admin == 1)
        {
            sprintf(msg, "Cannot Kick an admin\n");
            send_message_everyone_in_channel(msg, cli, 1);
            return;
        }
        if(strcmp(kicked_cli->name, name) == 0)
        {
            sprintf(msg, "%s has been kicked\n", kicked_cli->name);
            kicked_cli->leave_flag = 1;
            send_message_everyone_in_channel(msg, cli, 1);
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        send_message_everyone_in_channel(msg, cli, 1);
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
            send_message_everyone_in_channel(msg, cli, 1);
            cli->admin = 0;
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        send_message_everyone_in_channel(msg, cli, 1);
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
            send_message_everyone_in_channel(msg, cli, 1);
            cli->admin = 1;
        }
    }
    else{
        sprintf(msg, "Name not Found\n");
        send_message_everyone_in_channel(msg, cli, 1);
    }
    free(msg);
    return;
}

void change_name(client_t* cli, char *name)
{
    char msg[strlen(name) + 20];
    sprintf(msg, "%s's name changed to %s\n", cli->name, name);
    send_message_everyone_in_channel(msg, cli, 1);
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

channel_t* search_channel_by_name(char* channel_name)
{
    if(strcmp("general", channel_name) == 0)
    {
        return NULL;
    }
    for(int i = 0; i<MAX_CHANNELS; i++)
    {
        if(channels[i])
        {
            if(strcmp(channel_name, channels[i]->name) == 0)
            {
                return channels[i];
            }
        }
    }
    return NULL;
}

void change_channel(char* channel_name, client_t* cli)
{
    pthread_mutex_lock(&clients_mutex);

    channel_t* target_channel = search_channel_by_name(channel_name);
    channel_t* current_channel = search_channel_by_uid(cli->current_channel_uid);
    printf("%p\n", target_channel);

    if(strcmp(channel_name, "general") == 0)
    {
        cli->current_channel_uid = 0;
        bzero(cli->chn_name, strlen(cli->chn_name));
        bcopy("general", cli->chn_name, strlen("general"));
        for(int i = 0; i<MAX_CLIENTS_CHANNEL; i++)
        {
            if(current_channel)
            {
                if(current_channel->clients[i]->uid == cli->uid)
                {
                    current_channel->clients[i] = NULL;
                    char *msg = "Joined Channel Successfully\n";
                    send(cli->sockfd, msg, strlen(msg), 0);
                    break;
                }
            }
            else
            {
                printf("Error 405\n");
            }
        }
        pthread_mutex_unlock(&clients_mutex);   
        return;
    }
    if(target_channel)
    {
        if(cli->current_channel_uid == target_channel->channel_uid)
        {
            char *msg = "Already in that channel\n";
            send(cli->sockfd, msg, strlen(msg), 0);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
        int flag = 0;
        cli->current_channel_uid = target_channel->channel_uid;
        bzero(cli->chn_name, strlen(cli->chn_name));
        bcopy(target_channel->name, cli->chn_name, strlen(target_channel->name));
        for(int i = 0; i<MAX_CLIENTS_CHANNEL; i++)
        {
            if(!target_channel->clients[i])
            {
                target_channel->clients[i] = cli;
                flag = 1;
                char *msg = "Joined Channel Successfully\n";
                send(cli->sockfd, msg, strlen(msg), 0);
                break;
            }
        }
        if(flag == 0)
        {
            char *msg = "Cannot Join Channel : Channel Full\n";
            send(cli->sockfd, msg, strlen(msg), 0);
        }
    }
    else 
    {
        send(cli->sockfd, "Channel Doesn't Exist\n", strlen("Channel Doesn't Exist\n"), 0);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void create_channel(char* name, client_t* cli)
{
    pthread_mutex_lock(&clients_mutex);

    int flag = 0;
    for(int i = 0; i<MAX_CHANNELS; i++)
    {
        if(!channels[i])
        {
            channels[i] = (channel_t*) malloc(sizeof(channel_t));
            channels[i]->channel_uid = i+1;
            strcpy(channels[i]->name, name);
            pthread_mutex_unlock(&clients_mutex);
            send_message_everyone_in_channel("Channel Created Successfully\n", cli, 1);
           return;
        }
    }
    char *msg ="Cannot Create another Channel\n";
    send(cli->sockfd, msg, strlen(msg),0);
    pthread_mutex_unlock(&clients_mutex);
}

void print_channel_list(client_t* cli)
{
    for(int i = 0; i<MAX_CHANNELS; i++)
    {
        if(channels[i])
        {
            char msg[strlen(channels[i]->name) + 5];
            sprintf(msg, "%s\n", channels[i]->name);
            printf("%s\n", msg);
            send(cli->sockfd, msg, strlen(msg), 0);
        }
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
            kick_person(cmd+6, cli);
        }
        else
        {
            send_message_everyone_in_channel("Admin Rights Required\n", cli, 1);
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
            send_message_everyone_in_channel("Admin Rights Required\n", cli, 1);
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
            send_message_everyone_in_channel("Admin Rights Required\n", cli, 1);
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
    else if(strcmp(msg, "/join") == 0)
    {
        free(msg);
        change_channel(cmd+6, cli);
    }
    else if(strcmp(msg, "/create") == 0)
    {
        free(msg);
        if(cli->admin)
        {
            create_channel(cmd+8, cli);
        }
        else{
            send_message_everyone_in_channel("Admin Rights Required\n", cli, 1);
        }
    }
    else if(strcmp(msg, "/list") == 0)
    {
        print_channel_list(cli);
    }
    else{
        free(msg);
        send_message_everyone_in_channel("Wrong Command\n", cli, 1);
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

    //setting up general channel
    cli->current_channel_uid = 0;
    bcopy("general", cli->chn_name, strlen("general"));

    while(1)
    {
        if(cli->leave_flag)
        {
            break;
        }
        int recieve = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if(strcmp(buffer,"/leave") == 0)
        {
            sprintf(buffer, "%s has left the server\n", cli->name);
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
                printf("%s->%s : %s\n", cli->chn_name, cli->name, buffer);
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

    error(listen(listenFd, 5), "ERROR : Error on Listen\n" );

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