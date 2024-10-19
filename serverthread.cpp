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
#define QUEUE_SIZE 512 // Size of the task queue

// Structure to hold client information for threading
typedef struct {
    int client_fd;
    struct sockaddr_storage client_addr;
} client_info_t;

// Task queue structure
typedef struct {
    client_info_t *client_tasks[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} task_queue_t;

task_queue_t queue;

void init_task_queue() {
    queue.front = 0;
    queue.rear = 0;
    queue.count = 0;
    pthread_mutex_init(&queue.lock, NULL);
    pthread_cond_init(&queue.cond, NULL);
}

void enqueue_task(client_info_t *client_info) {
    pthread_mutex_lock(&queue.lock);
    if (queue.count < QUEUE_SIZE) {
        queue.client_tasks[queue.rear] = client_info;
        queue.rear = (queue.rear + 1) % QUEUE_SIZE;
        queue.count++;
        pthread_cond_signal(&queue.cond);
    } else {
        // If queue is full, reject the connection
        close(client_info->client_fd);
        free(client_info);
    }
    pthread_mutex_unlock(&queue.lock);
}

client_info_t *dequeue_task() {
    pthread_mutex_lock(&queue.lock);
    while (queue.count == 0) {
        pthread_cond_wait(&queue.cond, &queue.lock);
    }
    client_info_t *task = queue.client_tasks[queue.front];
    queue.front = (queue.front + 1) % QUEUE_SIZE;
    queue.count--;
    pthread_mutex_unlock(&queue.lock);
    return task;
}

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

    // Initialize the task queue
    init_task_queue();

    // Create a pool of worker threads
    pthread_t thread_pool[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&thread_pool[i], NULL, handle_client, NULL);
    }

    // Server loop: Continuously accept and process incoming connections
    while (1) {
        client_info_t *client_info = (client_info_t *)malloc(sizeof(client_info_t));
        if (!client_info) {
            perror("malloc");
            continue;
        }

        socklen_t addr_size = sizeof(client_info->client_addr);
        client_info->client_fd = accept(server_fd, (struct sockaddr *)&client_info->client_addr, &addr_size);
        if (client_info->client_fd == -1) {
            perror("accept");
            free(client_info);
            continue;
        }

        // Enqueue the task for the thread pool to process
        enqueue_task(client_info);
    }

    close(server_fd);
    return 0;
}
