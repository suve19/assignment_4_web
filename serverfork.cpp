
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

#define BACKLOG 10         // Maximum number of pending connections in the queue
#define BUFFER_SIZE 1024   // Size of the buffer to store received data

int server_fd; // Global server socket file descriptor

// Signal handler to reap zombie child processes
void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Signal handler for clean shutdown
void handle_sigint(int sig) {
    printf("Shutting down server...\n");
    close(server_fd);  // Close the server socket
    exit(0);  // Exit cleanly
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>:<PORT>\n", argv[0]);
        return 1;
    }

    // Split the IP and port from the argument
    char *ip_address = strtok(argv[1], ":");
    char *port_str = strtok(NULL, ":");
    if (!ip_address || !port_str) {
        fprintf(stderr, "Invalid format. Expected format: <IP>:<PORT>\n");
        return 1;
    }
    int port = atoi(port_str);

    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;
    struct sigaction sa;

    // Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set the socket option SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);
    server_addr.sin_port = htons(port);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Set up SIGINT handler for clean shutdown
    signal(SIGINT, handle_sigint);

    printf("Server is running on %s:%d\n", ip_address, port);

    // Server loop: Continuously accept and handle incoming connections
    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {  // Child process
            close(server_fd);  // Child does not need the listening socket
            char buffer[BUFFER_SIZE];
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0) {
                perror("recv");
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            buffer[bytes_received] = '\0';

            char method[16], url[256];
            sscanf(buffer, "%s %s", method, url);

            // Reject URLs with more than 1 slash
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

        close(client_fd);  // Parent doesn't need this client socket
    }

    close(server_fd);
    return 0;
}
