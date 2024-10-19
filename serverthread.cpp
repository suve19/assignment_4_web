#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#define BACKLOG 10
#define MAX_THREADS 8  // Number of threads in the pool

void *handle_client(void *arg) {
    // Placeholder for handling client requests
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    const char *ip_address = argv[1];
    int port = atoi(argv[2]);
    int server_fd;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Resolve DNS and support both IPv4 and IPv6
    if (getaddrinfo(ip_address, argv[2], &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    // Create the server socket
    if ((server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        perror("socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Bind the socket
    if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        close(server_fd);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    // Start listening for incoming connections
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on %s:%d\n", ip_address, port);

    // Create a pool of worker threads
    pthread_t thread_pool[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&thread_pool[i], NULL, handle_client, NULL);
    }

    // Server loop: Continuously accept and process incoming connections
    while (1) {
        int client_fd;
        socklen_t addr_size;
        struct sockaddr_storage client_addr;

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        // Placeholder for handling the client connection
    }

    close(server_fd);
    return 0;
}
