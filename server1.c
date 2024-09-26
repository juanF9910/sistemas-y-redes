#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int uid = 10;

void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *message, int uid, char *ip_addr) {
    char full_message[BUFFER_SIZE];
    snprintf(full_message, sizeof(full_message), "%s: %s", ip_addr, message);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid != uid) {
            if (send(clients[i]->sockfd, full_message, strlen(full_message), 0) < 0) {
                perror("ERROR: write to descriptor failed");
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    //int leave_flag = 0;
    client_t *cli = (client_t *)arg;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli->address.sin_addr, client_ip, sizeof(client_ip));
    printf("Client connected with IP: %s\n", client_ip);

    while (1) {
        int receive = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            buffer[receive] = '\0';
            //if (strlen(buffer) > 0) {
                printf("Message from %s: %s\n", client_ip, buffer); 
                send_message(buffer, cli->uid, client_ip);
            //}
        } else if (receive == 0 || strcmp(buffer, "exit") == 0) {
            printf("Client with IP %s has disconnected\n", client_ip);
            break;
            //leave_flag = 1;
        } else {
            perror("ERROR: -1");
            break;
            //leave_flag = 1;
        }
        bzero(buffer, BUFFER_SIZE);
        //if (leave_flag) break;
    }

    close(cli->sockfd);
    remove_client(cli->uid);
    free(cli);
    pthread_detach(pthread_self());
    return NULL;
}

int main() {
    int sockfd, new_sock;
    struct sockaddr_in server_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR: bind failed");
        return EXIT_FAILURE;
    }

    if (listen(sockfd, 10) < 0) {
        perror("ERROR: listen failed");
        return EXIT_FAILURE;
    }

    printf("=== Server is running ===\n");

    while (1) {
        new_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if ((uid - 10) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected connection.\n");
            close(new_sock);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = new_sock;
        cli->uid = uid++;
        add_client(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);
    }

    return EXIT_SUCCESS;
}