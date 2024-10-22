#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

#define BACKLOG 10  // Maximum number of pending connections in the queue
#define BUFFER_SIZE 1024  // Size of the buffer to store received data

/**
 * Main function: Starts the HTTP server, listens for incoming connections, and serves requested files.
 * 
 * @param argc Number of arguments passed (expects 2 arguments: IP address and port).
 * @param argv Array of arguments passed (IP address and port).
 * @return int Returns 0 on successful execution, 1 if there are incorrect arguments, or exits on failure.
 */
int main(int argc, char *argv[]) {
    // Check for valid argument count (IP and Port)
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    // Get IP address and port from command line arguments
    const char *ip_address = argv[1];
    int port = atoi(argv[2]);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;

    // Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    server_addr.sin_family = AF_INET;  // IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip_address);  // Convert IP address
    server_addr.sin_port = htons(port);  // Convert port number to network byte order
    memset(&(server_addr.sin_zero), '\0', 8);  // Zero out the rest of the struct

    // Bind the server socket to the specified IP address and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on %s:%d\n", ip_address, port);

    // Server loop: Continuously accept and handle incoming connections
    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        // Accept an incoming connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            perror("accept");
            continue;  // Continue to next connection if accept fails
        }

        // Fork a child process to handle the client request
        if (fork() == 0) {  // Child process
            close(server_fd);  // Child does not need the listening socket

            // Buffer to store the client's request
            char buffer[BUFFER_SIZE];
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0) {
                perror("recv");
                close(client_fd);
                exit(EXIT_FAILURE);  // Exit child process on failure
            }

            buffer[bytes_received] = '\0';  // Null-terminate the received data

            // Parse the HTTP request method and URL
            char method[16], url[256];
            sscanf(buffer, "%s %s", method, url);

            // Reject requests with URLs containing more than 3 slashes
            size_t slash_count = 0;
            for (size_t i = 0; i < strlen(url); i++) {
                if (url[i] == '/') {
                    slash_count++;
                }
            }
            if (slash_count > 3) {
                const char *error_message = "HTTP/1.1 403 Forbidden\r\n\r\n";
                send(client_fd, error_message, strlen(error_message), 0);
                close(client_fd);
                exit(EXIT_SUCCESS);  // Exit after sending error response
            }

            // Handle only GET and HEAD methods; reject other methods
            if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
                const char *not_implemented = "HTTP/1.1 501 Not Implemented\r\n\r\n";
                send(client_fd, not_implemented, strlen(not_implemented), 0);
                close(client_fd);
                exit(EXIT_SUCCESS);  // Exit after sending error response
            }

            // Determine the requested file path
            char file_path[256];
            if (strcmp(url, "/") == 0) {
                strcpy(file_path, "./index.html");  // Serve index.html for root URL
            } else {
                snprintf(file_path, sizeof(file_path), ".%s", url);  // Serve file based on URL path
            }

            // Attempt to open the requested file
            int file_fd = open(file_path, O_RDONLY);
            if (file_fd < 0) {
                const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, not_found, strlen(not_found), 0);
            } else {
                const char *ok_response = "HTTP/1.1 200 OK\r\n\r\n";
                send(client_fd, ok_response, strlen(ok_response), 0);

                // If the request is GET, send the file content
                if (strcmp(method, "GET") == 0) {
                    char file_buffer[BUFFER_SIZE];
                    int read_bytes;
                    while ((read_bytes = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
                        send(client_fd, file_buffer, read_bytes, 0);
                    }
                }

                close(file_fd);  // Close the file after sending its content
            }

            close(client_fd);  // Close the connection to the client
            exit(EXIT_SUCCESS);  // Exit child process after handling the request
        }

        // Parent process
        close(client_fd);  // Parent doesn't need this client socket
    }

    close(server_fd);  // Close the server socket before exiting
    return 0;
}
