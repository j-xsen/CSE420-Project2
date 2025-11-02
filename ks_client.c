//
// Created by jax on 10/31/25.
//
#define MAXDIRPATH 1024
#define MAXKEYWORD 256
#define MAXLINESIZE 1024
#define MAXOUTSIZE 2048

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

struct msgbuf
{
    long mtype;
    char mtext[MAXLINESIZE+MAXKEYWORD+2];
};

void sendSearch(const char* search, const int msg_id)
{
    struct msgbuf buf;
    buf.mtype = 1;
    strncpy(buf.mtext, search, sizeof(buf.mtext)-1);
    buf.mtext[sizeof(buf.mtext)-1]='\0';
    msgsnd(msg_id, &buf, strlen(buf.mtext),0);
}

int closeConnection(const int msg_id)
{
    if (msgctl(msg_id, IPC_RMID, 0) == -1)
    {
        perror("msgctl");
        return 1;
    }

    return 0;
}

void* monitorResponse(void* arg)
{
    int msg_id = *(int*)arg;
    struct msgbuf* buf = malloc(sizeof(struct msgbuf));
    while (1)
    {
        memset(buf->mtext, 0, sizeof(buf->mtext));

        ssize_t received = msgrcv(msg_id, buf, sizeof(buf->mtext), 0, 0);
        if (received==-1)
        {
            perror("msgrcv");
            free(buf);
            return NULL;
        }
        if (strlen(buf->mtext)>0)
        {
            if (strcmp(buf->mtext,"exit")==0)
            {
                closeConnection(msg_id);
                free(buf);
                return NULL;
            }
            printf("%s\n", buf->mtext);
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

    char pid_str[16];
    sprintf(pid_str, "%d", getpid());

    // keys
    const key_t server_key = ftok("ks_server.c", 67);
    const key_t client_key = ftok(argv[2], atoi(pid_str));

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

    size_t buf_size = strlen(argv[1]) + strlen(argv[2]) + strlen(pid_str) + 3;
    char* buf = malloc(buf_size);
    snprintf(buf, buf_size, "%s:%s:%s", argv[1], argv[2], pid_str);
    pthread_t response_thread;
    if (pthread_create(&response_thread, NULL, monitorResponse, (void*)&client_msg_id) != 0)
    {
        perror("pthread_create");
        free(buf);
        return 1;
    }

    // send message
    sendSearch(buf,server_msg_id);

    pthread_join(response_thread, NULL);

    free(buf);

    // close message queue
    closeConnection(client_msg_id);

    return 0;
}
