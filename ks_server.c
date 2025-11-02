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
    char* process_id;
} Query;

struct msgbuf
{
    long mtype;
    char mtext[MAXLINESIZE+MAXKEYWORD+2];
};

void* analyzeFile(void* arg)
{
    Query* cur_qry = (Query*)arg;
    size_t size_of_path = strlen(cur_qry->location) + strlen(cur_qry->file_name) + 2;
    char* full_path = calloc(1, size_of_path);
    snprintf(full_path, size_of_path, "%s/%s", cur_qry->location, cur_qry->file_name);

    printf("Searching %s for %s\n", full_path, cur_qry->keyword);

    FILE* file = fopen(full_path,"r");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }

    const key_t client_key = ftok(cur_qry->location, atoi(cur_qry->process_id));
    int client_msg_id = msgget(client_key, 0);
    if (client_msg_id == -1)
    {
        perror("msgget analyze");
    }

    char buf[MAXLINESIZE];
    int msg_sent = 0;
    while (fgets(buf,sizeof(buf), file) != NULL)
    {
        char* buf_bu = strdup(buf);
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
                struct msgbuf* msg = malloc(sizeof(struct msgbuf));
                memset(msg, 0, sizeof(struct msgbuf));
                msg->mtype = 1;
                snprintf(msg->mtext, sizeof(msg->mtext), "%s:%s", cur_qry->keyword, buf_bu);
                printf("Sending %s\n",msg->mtext);
                msgsnd(client_msg_id,msg,strlen(msg->mtext)+1,0);
                free(msg);
                msg_sent=1;
                break;
            }
            token = strtok_r(NULL, " ", &save_ptr);
        }
        free(buf_bu);
    }
    struct msgbuf* msg_buf = malloc(sizeof(struct msgbuf));
    msg_buf->mtype=1;
    strcpy(msg_buf->mtext,"exit");
    msgsnd(client_msg_id,msg_buf,strlen(msg_buf->mtext)+1,0);
    free(msg_buf);

    free(cur_qry->location);
    free(cur_qry->file_name);
    free(cur_qry->process_id);
    free(cur_qry->keyword);
    free(cur_qry);
    free(full_path);

    fclose(file);

    return NULL;
}


void clientMessage(char* keyword, const char* full_dir, char* process_id)
{
    struct dirent *entry;
    DIR *dir = opendir(full_dir);

    if (dir == NULL)
    {
        printf("full_dir: %s\n", full_dir);
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
    int thread_created[file_count];
    for (int i=0;i<file_count;i++)
    {
        Query* new_qry = malloc(sizeof(Query));
        if (!new_qry)
        {
            perror("calloc failed");
            exit(1);
        }
        new_qry->location = strdup(full_dir);
        new_qry->file_name = strdup(files[i]);
        new_qry->keyword = strdup(keyword);
        new_qry->process_id = strdup(process_id);
        if (pthread_create(&threads[i], NULL, analyzeFile, (void*)new_qry) == 0)
        {
            thread_created[i] = 1;
        } else {
            perror("pthread_create");
        }
        pthread_join(threads[i], NULL);
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
    struct msgbuf* buf = malloc(sizeof(struct msgbuf)); // msg buffer
    while (server_status)
    {
        // clear buffer
        memset(buf->mtext, 0, sizeof(buf->mtext));

        // check for msg
        msgrcv(msgid, buf, sizeof(buf->mtext), 0, 0);
        if (strlen(buf->mtext) > 0)
        {
            char *saveptr;
            char *keyword = strtok_r(buf->mtext, ":", &saveptr);
            char *full_path = strtok_r(NULL, ":", &saveptr);
            char *process_id = strtok_r(NULL, ":", &saveptr);
            // check if exit
            if (strcmp(keyword, "exit") == 0)
            {
                // clean up
                if (msgctl(msgid, IPC_RMID, 0) == -1)
                {
                    perror("msgctl");
                    free(buf);
                    return(1);
                }

                // exit
                server_status = 0;
            } else
            {
                if (!keyword || !full_path)
                {
                    printf("Invalid message format.\n");
                    printf("mtext: %s\n", buf->mtext);
                    continue;
                }

                pid_t pid = fork();
                if (pid == 0)
                    clientMessage(keyword, full_path, process_id);
                else if (pid < 0)
                    perror("fork failed");
            }
        }
        sleep(1);
    }

    free(buf);

    return 0;
}
