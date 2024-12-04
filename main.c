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

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
        else 
        {
            printf("Processing new request: \n");
            buffer[bytes_read] = '\0';
            puts(buffer);
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), connecting_addr, sizeof connecting_addr);
        printf("server: got connection from %s\n", connecting_addr);

        char *tmp = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                         "<!DOCTYPE html>\r\n"
                         "<html>\r\n"
                         "<head>\r\n"
                         "<title>Httpc</title>\r\n"
                         "</head>\r\n"
                         "<body>Hello Reagan</body>\r\n"
                         "</html>\r\n";
        send(new_socket, tmp, strlen(tmp), 0);

        close(new_socket);
        memset(buffer, 0, MAXSIZE);
    }

    freeaddrinfo(server_info);
    if (shutdown(server_socket, SHUT_RDWR) < 0)
    {
        perror("Shutdown error: ");
    }
    close(server_socket);
    return 0;
}

