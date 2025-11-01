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
#include <dirent.h>
#include <pthread.h>

typedef struct
{
    char* location;
    char* file_name;
    char* keyword;
} Query;

void* analyzeFile(void* arg)
{
    Query* cur_qry = (Query*)arg;
    char* full_path = strdup(cur_qry->location);
    strcat(full_path,"/");
    strcat(full_path,cur_qry->file_name);

    FILE* file = fopen(full_path,"r");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }

    const key_t client_key = ftok(cur_qry->location, 67);
    int client_msg_id = msgget(client_key, 0);
    if (client_msg_id == -1)
    {
        perror("msgget analyze");
    }

    char buf[MAXLINESIZE];
    while (fgets(buf,sizeof(buf), file) != NULL)
    {
        char* full_line = strdup(buf);
        // split up by space
        char* save_ptr;
        char* token = strtok_r(buf, " ", &save_ptr);
        while (token != NULL)
        {
            if (token[strlen(token)-1]=='\n')
            {
                token[strlen(token)-1]='\0';
            }
            if (strcmp(token, cur_qry->keyword) == 0)
            {
                // send message to client
                char* send_str = strdup(cur_qry->keyword);
                strcat(send_str,":");
                strcat(send_str,full_line);
                msgsnd(client_msg_id,send_str,strlen(send_str)+1,0);
                break;
            }
            token = strtok_r(NULL, " ", &save_ptr);
        }
    }

    char* depart = "quit";
    msgsnd(client_msg_id,depart,strlen(depart)+1,0);

    free(cur_qry);

    fclose(file);

    return NULL;
}


void clientMessage(char* keyword, const char* full_dir)
{
    struct dirent *entry;
    DIR *dir = opendir(full_dir);

    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    int file_count = 0; // number of threads
    char** files = NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        // skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // skip directories
        if (entry->d_type == DT_DIR)
            continue;

        // add slot to files
        char** new_files = realloc(files, sizeof(char*)*(file_count+1));
        if (new_files==NULL)
        {
            perror("calloc failed");
            free(files);
            exit(1);
        }
        files = new_files;

        // fill slot with string
        files[file_count] = strdup(entry->d_name);
        if (files[file_count] == NULL)
        {
            perror("strdup failed");
            free(files);
            exit(1);
        }

        file_count++;
    }

    pthread_t threads[file_count];
    for (int i=0;i<file_count;i++)
    {
        Query* new_qry = malloc(sizeof(Query));
        new_qry->location = strdup(full_dir);
        new_qry->file_name = strdup(files[i]);
        new_qry->keyword = keyword;
        if (pthread_create(&threads[i], NULL, analyzeFile, (void*)new_qry))
        {
            perror("pthread_create");
            exit(1);
        }
    }

    for (int i=0;i<file_count;i++)
    {
        if (pthread_join(threads[i],NULL))
        {
            perror("pthread_join");
            exit(1);
        }
    }

    for (int i = 0; i < file_count; i++)
    {
        free(files[i]);
    }
    free(files);

    closedir(dir);
}

int main()
{
    // unioue key
    key_t key = ftok("ks_server.c", 67);

    // create or read message queue
    int msgid = msgget(key, 0666 | IPC_CREAT);

    if (msgid == -1)
    {
        perror("msgget main");
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
        {
            // check if exit
            if (strcmp(buf, "exit") == 0)
            {
                // clean up
                if (msgctl(msgid, IPC_RMID, 0) == -1)
                {
                    perror("msgctl");
                    return(1);
                }

                // exit
                server_status = 0;
            } else
            {
                char *saveptr;
                char *keyword = strtok_r(buf, ":", &saveptr);
                char *full_path = strtok_r(NULL, ":", &saveptr);

                if (!keyword || !full_path)
                {
                    printf("Invalid message format.\n");
                    continue;
                }

                pid_t pid = fork();
                if (pid == 0)
                    clientMessage(keyword, full_path);
                else if (pid < 0)
                    perror("fork failed");
            }
        }
        sleep(1);
    }

    return 0;
}
