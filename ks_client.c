//
// Created by jax on 10/31/25.
//
#define MAXDIRPATH 1024
#define MAXKEYWORD 256
#define MAXLINESIZE 1024
#define MAXOUTSIZE 2048

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    if (argc != 3)
    {
        printf("Usage: ks_client <keyword> <dirpath>\n");
        printf("Arguments expected: 2\n");
        printf("Arguments received: %i\n", argc);
    }

    // server key
    key_t key = ftok("ks_server.c", 67);
    key_t client_key = ftok("ks_client.c", 67);

    // test send message
    // get msg id
    int msgid = msgget(key, 0);

    if (msgid == -1)
    {
        perror("msgget");
        return(1);
    }

    char buf[1024] = "Hello World!";
    msgsnd(msgid, &buf,sizeof(buf), 0);

    return 0;
}
