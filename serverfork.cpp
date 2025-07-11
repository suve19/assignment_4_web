#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>

#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_HTTP_HEADERS 8192  // Maximum HTTP header size

int server_fd; // Global server socket file descriptor

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigint(int sig) {
    printf("Shutting down server...\n");
    close(server_fd);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP/Hostname>:<PORT>\n", argv[0]);
        return 1;
    }

    char *host = strtok(argv[1], ":");
    char *port_str = strtok(NULL, ":");
    if (!host || !port_str) {
        fprintf(stderr, "Invalid format. Expected format: <IP/Hostname>:<PORT>\n");
        return 1;
    }

    struct addrinfo hints, *res, *p;
    int opt = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("setsockopt");
            close(server_fd);
            continue;
        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(server_fd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to bind to any address\n");
        return 1;
    }

    freeaddrinfo(res);

    printf("Server is running on %s:%s\n", host, port_str);

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    signal(SIGINT, handle_sigint);

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            close(server_fd);
            
            char buffer[BUFFER_SIZE];
            char full_request[MAX_HTTP_HEADERS] = {0};
            size_t total_received = 0;
            int message_complete = 0;

            while (!message_complete && total_received < sizeof(full_request) - 1) {
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received < 0) {
                    perror("recv");
                    close(client_fd);
                    exit(EXIT_FAILURE);
                } else if (bytes_received == 0) {
                    close(client_fd);
                    exit(EXIT_SUCCESS);
                }

                memcpy(full_request + total_received, buffer, bytes_received);
                total_received += bytes_received;
                full_request[total_received] = '\0';

                if (strstr(full_request, "\r\n\r\n") != NULL) {
                    message_complete = 1;
                }
            }

            if (!message_complete) {
                const char *error_message = "HTTP/1.1 413 Payload Too Large\r\n\r\n";
                send(client_fd, error_message, strlen(error_message), 0);
                close(client_fd);
                exit(EXIT_SUCCESS);
            }

            char method[16], url[256];
            sscanf(full_request, "%s %s", method, url);

            size_t slash_count = 0;
            for (size_t i = 0; i < strlen(url); i++) {
                if (url[i] == '/') {
                    slash_count++;
                }
            }
            if (slash_count > 1) {
                const char *error_message = "HTTP/1.1 403 Forbidden\r\n\r\n";
                send(client_fd, error_message, strlen(error_message), 0);
                close(client_fd);
                exit(EXIT_SUCCESS);
            }

            if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
                const char *not_implemented = "HTTP/1.1 501 Not Implemented\r\n\r\n";
                send(client_fd, not_implemented, strlen(not_implemented), 0);
                close(client_fd);
                exit(EXIT_SUCCESS);
            }

            char file_path[512];
            snprintf(file_path, sizeof(file_path), ".%s", url);

            int file_fd = open(file_path, O_RDONLY);
            if (file_fd < 0) {
                const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, not_found, strlen(not_found), 0);
            } else {
                const char *ok_response = "HTTP/1.1 200 OK\r\n\r\n";
                send(client_fd, ok_response, strlen(ok_response), 0);

                if (strcmp(method, "GET") == 0) {
                    char file_buffer[BUFFER_SIZE];
                    int read_bytes;
                    while ((read_bytes = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
                        send(client_fd, file_buffer, read_bytes, 0);
                    }
                }
                close(file_fd);
            }

            close(client_fd);
            exit(EXIT_SUCCESS);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}