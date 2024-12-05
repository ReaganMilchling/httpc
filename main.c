#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "8080"
#define BACKLOG 10
#define MAXSIZE 2048
#define PATHMAX 512
#define MAXFILESIZE 4096

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool is_home_dir(const char* dir)
{
    if (dir[0] == '\0') return true;
    return false;
}

char* read_file(const char* file_path)
{
    FILE* file = fopen(file_path, "rb");

    if (file != NULL)
    {
        char* buffer = NULL;
        if (fseek(file, 0L, SEEK_END) == 0)
        {
            long len = ftell(file);
            if (fseek(file, 0L, SEEK_SET) != 0)
            {
                printf("failed to seek to start\n");
            }

            buffer = malloc(sizeof(char) * (len+1));

            int s = fread(buffer, sizeof(char), len, file);
            if (ferror(file))
            {
                perror("file failed to load");
            }
            else 
            {
                buffer[s] = '\0';
            }
        }
        else {
            printf("failed to seek\n");
        }
        fclose(file);
        return buffer;
    }
    printf("File was null\n");
    return NULL;
}

int main(void)
{
    char cwd[PATHMAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Starting HTTP server, serving: %s\n", cwd);
    } else {
        perror("getcwd() error");
        return(EXIT_FAILURE);
    }

    char buffer[MAXSIZE] = { 0 };
    char ptcl[10];
    char path[PATHMAX];
    char end[100];
    char connecting_addr[INET6_ADDRSTRLEN];
    char ip4[INET_ADDRSTRLEN] = "127.0.0.1";

    //-----------BEGIN SERVER STARTUP-----------
    int addr_status;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    int server_socket, new_socket;

    if ((addr_status = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0)
    {
        perror("getaddrinfo failure: ");
        return(EXIT_FAILURE);
    }
    if ((server_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) < 0) 
    {
        perror("Socket init error: ");
        return(EXIT_FAILURE);
    }
    if (bind(server_socket, server_info->ai_addr, server_info->ai_addrlen) < 0) 
    {
        perror("Server bind error: ");
        return(EXIT_FAILURE);
    }
    if (listen(server_socket, BACKLOG) < 0) 
    {
        perror("Listen setup error: ");
        return(EXIT_FAILURE);
    }
    //-----------END SERVER STARTUP-----------
whileLoop: while(1) {
        if ((new_socket = accept(server_socket, (struct sockaddr*)&their_addr, &addr_size)) < 0) 
        {
            perror("Accept error: ");
            goto whileLoop;
        }

        ssize_t bytes_read = read(new_socket, buffer, MAXSIZE);
        if ( bytes_read < 0)
        {
            perror("Read error: ");
            goto whileLoop;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), connecting_addr, sizeof connecting_addr);

        buffer[bytes_read] = '\0';

        const char* start = buffer;
        int ptcl_index = 0;
        const char* path_start = strchr(buffer, '/');
        int path_index = 0;
        const char* end_start = strchr(path_start, ' ');

        while (start != path_start)
        {
            ptcl[ptcl_index++] = *start;
            start++;
        }
        ptcl[ptcl_index] = '\0';
        path_start++;

        while (path_start != end_start)
        {
            path[path_index++] = *path_start;
            path_start++;
        }
        path[path_index] = '\0';

        printf("Request:%s:%s:%s\n", ptcl, path, connecting_addr);

        char* file_text;
        if (is_home_dir(path))
        {
            printf("defaulting to index.html");
            file_text = read_file("index.html");
        }
        else
        {
            file_text = read_file(path);
        }

        if (file_text == NULL)
        {
            char *fail = "HTTP/1.1 404 Not found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n\
<HTML><HEAD><TITLE>File not found</TITLE></HEAD><BODY><H2>File not found</H2></BODY></HTML>";
            printf("Serving 404\n");
            send(new_socket, fail, strlen(fail), 0);
        }
        else
        {
            int index = 0;
            char * mime;
            const char* extension = strchr(path, '.');
            printf("extension:%s\n", extension);
            if (strcmp(extension, ".css") == 0) {
                mime = "text/css";
            } else if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0) {
                mime = "text/html";
            } else if (strcmp(extension, ".webp") == 0) {
                mime = "text/webp";
            } else if (strcmp(extension, ".csv") == 0) {
                mime = "text/csv";
            } else if (strcmp(extension, ".json") == 0) {
                mime = "application/json";
            } else if (strcmp(extension, ".jpeg") == 0 || strcmp(extension, ".jpg") == 0) {
                mime = "image/jpeg";
            } else if (strcmp(extension, ".gif") == 0) {
                mime = "image/gif";
            } else if (strcmp(extension, ".png") == 0) {
                mime = "image/png";
            } else {
                mime = "text/plain";
            }

            char *tmp = "HTTP/1.1 200 OK\r\nContent-Type: "; 
            char *follow = ";\r\n\r\n";
            char* response = malloc(strlen(tmp) + strlen(file_text) + strlen(mime) + strlen(follow) + 1);
            strcpy(response, tmp);
            strcat(response, mime);
            strcat(response, follow);
            strcat(response, file_text);
            printf("Serving:content length-%li:Mime-%s\n", strlen(response), mime);
            send(new_socket, response, strlen(response), 0);
            if (file_text != NULL) free(file_text);
            if (response != NULL) free(response);
        }

        close(new_socket);
        memset(buffer, 0, MAXSIZE);
        memset(ptcl, 0, 10);
        memset(path, 0, PATHMAX);
        memset(end, 0, 100);
    }

    freeaddrinfo(server_info);
    if (shutdown(server_socket, SHUT_RDWR) < 0)
    {
        perror("Shutdown error: ");
    }
    close(server_socket);
    return 0;
}

