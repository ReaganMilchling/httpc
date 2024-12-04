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
    puts(dir);
    if (dir[0] == '/' && dir[1] == '\0') return true;
    return false;
}

void read_file(char* buffer, const char* file_path)
{
    FILE* file = fopen(file_path, "rb");

    if (file != NULL)
    {
        fseek(file, 0, SEEK_END);
        long len = ftell(file);
        fseek(file, 0, SEEK_SET);

        buffer = malloc(len);

        if (buffer)
        {
            fread(buffer, 1, len, file);
        }
        fclose(file);
    }
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

    char* file = 0;

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

        while (path_start != end_start)
        {
            path[path_index++] = *path_start;
            path_start++;
        }
        path[path_index] = '\0';


        printf("Received req: \"%s %s\" from %s\n", ptcl, path, connecting_addr);

        if (is_home_dir(path))
        {
            printf("defaulting to index.html");
            read_file(file, "/index.html");
        }
        else
        {
            read_file(file, path);
        }

        char *tmp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
        if (!file)
        {
            perror("could not open file");
            goto whileLoop;
        }

        send(new_socket, file, strlen(file), 0);
        //send(new_socket, tmp, strlen(tmp), 0);

        close(new_socket);
        memset(buffer, 0, MAXSIZE);
        memset(ptcl, 0, 10);
        memset(path, 0, PATHMAX);
        memset(end, 0, 100);
        free(file);
        file = 0;
    }

    freeaddrinfo(server_info);
    if (shutdown(server_socket, SHUT_RDWR) < 0)
    {
        perror("Shutdown error: ");
    }
    close(server_socket);
    return 0;
}

