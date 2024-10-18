#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

#define BACKLOG 10
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    const char *ip_address = argv[1];
    int port = atoi(argv[2]);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;

    // Create the socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);
    server_addr.sin_port = htons(port);
    memset(&(server_addr.sin_zero), '\0', 8);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on %s:%d\n", ip_address, port);

    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }

        printf("Server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));

        // Receive and parse request
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            perror("recv");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        // Basic parsing of the request
        char method[16], url[256];
        sscanf(buffer, "%s %s", method, url);

        // Handle only GET and HEAD methods
        if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
            const char *not_implemented = "HTTP/1.1 501 Not Implemented\r\n\r\n";
            send(client_fd, not_implemented, strlen(not_implemented), 0);
            close(client_fd);
            continue;
        }

        // Determine the requested file
        char file_path[256];
        if (strcmp(url, "/") == 0) {
            strcpy(file_path, "./index.html");  // Serve index.html by default
        } else {
            snprintf(file_path, sizeof(file_path), ".%s", url);
        }

        // Open and serve the file
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
    }

    close(server_fd);
    return 0;
}
