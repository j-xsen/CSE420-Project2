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


void* analyzeFile(void* arg)
{
    char* thread_id = (char*)arg;
    printf("From thread %s\n",thread_id);
    return NULL;
}


void clientMessage(char* keyword, char* full_dir)
{
    printf("Keyword: %s\n", keyword);
    printf("Full path: %s\n", full_dir);

    struct dirent *entry;
    DIR *dir = opendir(full_dir);

    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    printf("Files in directory:\n");
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
        char* full_path = strdup(full_dir);
        strcat(full_path,"/");
        strcat(full_path,files[i]);
        if (pthread_create(&threads[i], NULL, analyzeFile, (void*)full_path))
        {
            perror("pthread_create");
            exit(1);
        }
        printf("%s\n", files[i]);
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
        {
            printf("Received message: %s\n", buf);

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
