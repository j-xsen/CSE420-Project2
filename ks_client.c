//
// Created by jax on 10/31/25.
//
#define MAXDIRPATH 1024
#define MAXKEYWORD 256
#define MAXLINESIZE 1024
#define MAXOUTSIZE 2048

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

void sendSearch(char* search, int msg_id)
{
    msgsnd(msg_id, search, strlen(search),0);
}

void* monitorResponse(void* arg)
{
    int msg_id = *(int*)arg;
    char buf[1024];
    while (1)
    {
        memset(buf, 0, sizeof(buf));

        msgrcv(msg_id, buf, sizeof(buf), 0, 0);
        if (strlen(buf)>0)
        {
            if (strcmp(buf,"quit")==0)
            {
                return NULL;
            }
            printf("%s\n", buf);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3)
    {
        printf("Usage: ks_client <keyword> <dirpath>\n");
        printf("Arguments expected: 2\n");
        printf("Arguments received: %i\n", argc);
    }

    // keys
    const key_t server_key = ftok("ks_server.c", 67);
    const key_t client_key = ftok(argv[2], 67);

    // msq_ids
    const int server_msg_id = msgget(server_key, 0);
    if (server_msg_id == -1)
    {
        perror("msgget server");
        return 1;
    }

    const int client_msg_id = msgget(client_key, 0666 | IPC_CREAT);
    if (client_msg_id == -1)
    {
        perror("msgget client");
        return 1;
    }

    char buf[1024];
    strcpy(buf, argv[1]);
    strcat(buf, ":");
    strcat(buf, argv[2]);

    pthread_t response_thread;
    if (pthread_create(&response_thread, NULL, monitorResponse, (void*)&client_msg_id) != 0)
    {
        perror("pthread_create");
        return 1;
    }

    // send message
    sendSearch(buf,server_msg_id);

    pthread_join(response_thread, NULL);

    // close message queue
    if (msgctl(client_msg_id, IPC_RMID, 0) == -1)
    {
        perror("msgctl");
        return 1;
    }

    return 0;
}
