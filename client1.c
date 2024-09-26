#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void *receive_message(void *sockfd) {
    int sock = *((int *)sockfd);
    char message[BUFFER_SIZE];

    while (1) {
        int receive = recv(sock, message, BUFFER_SIZE, 0);
        if (receive > 0) {
            message[receive] = '\0'; // Ensure null termination
            printf("%s\n", message); // Print received message (from other clients)
        } else if (receive == 0) {
            printf("Server disconnected.\n");
            break;
        } else {
            perror("Error receiving message");
        }
        bzero(message, BUFFER_SIZE); // Clear buffer
    }

    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error in socket creation");
        return EXIT_FAILURE;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // Server port
    server_addr.sin_addr.s_addr = inet_addr("10.10.110.213"); // Change IP if needed

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }

    // Create thread to receive messages from the server
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_message, &sockfd);

    // Loop to send messages
    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer) - 1] = '\0'; // Remove newline at the end

        if (strcmp(buffer, "exit") == 0) {
            send(sockfd, buffer, strlen(buffer), 0);
            break;
        }

        // Send message to the server
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("Error sending message");
            break;
        }

        bzero(buffer, BUFFER_SIZE); // Clear buffer
    }

    // Close connection
    close(sockfd);
    printf("Disconnected from server.\n");

    return EXIT_SUCCESS;
}
