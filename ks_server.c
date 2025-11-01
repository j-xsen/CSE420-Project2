//
// Created by jax on 10/31/25.
//
#define MAXDIRPATH 1024
#define MAXKEYWORD 256
#define MAXLINESIZE 1024
#define MAXOUTSIZE 2048

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

int main()
{
    printf("Starting server...\n\n");

    // unioue key
    key_t key = ftok("ks_server.c", 67);

    // create or read message queue
    int msgid = msgget(key, 0666 | IPC_CREAT);

    if (msgid == -1)
    {
        perror("msgget");
        return(1);
    }

    int server_status = 1;
    char buf[1024]; // msg buffer
    while (server_status)
    {
        // clear buffer
        memset(buf, 0, sizeof(buf));

        // check for msg
        msgrcv(msgid, buf, sizeof(buf), 0, 0);
        if (strlen(buf) > 0)
            printf("Received message: %s\n", buf);
        if (strcmp(buf, "quit") == 0)
        {
            // clean up
            if (msgctl(msgid, IPC_RMID, 0) == -1)
            {
                perror("msgctl");
                return(1);
            }

            // exit
            return 0;
        }
        sleep(1);
    }

    return 0;
}
