#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 4096 // Aumentar el tamaño del buffer para manejar paquetes más grandes

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int uid = 10;

// Función para agregar cliente
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

// Función para eliminar cliente
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

// Función para enviar mensaje a todos los clientes
void send_message(char *message, int uid, char *ip_addr){
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

// Función para manejar cliente
void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    int leave_flag = 0;
    client_t *cli = (client_t *)arg;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli->address.sin_addr, client_ip, sizeof(client_ip));
    printf("Client connected with IP: %s\n", client_ip);

    while (1) {
        int receive = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            buffer[receive] = '\0';
            printf("Message from %s: %s\n", client_ip, buffer);
            if (strlen(buffer) > 0) {
                //printf("Message from %s: %s\n", client_ip, buffer);
                send_message(buffer, cli->uid, client_ip);
            }
        } else if (receive == 0 || strcmp(buffer, "exit") == 0) {
            printf("Client with IP %s has disconnected\n", client_ip);
            leave_flag = 1;
        } else {
            perror("ERROR: -1");
            leave_flag = 1;
        }
        bzero(buffer, BUFFER_SIZE);
        if (leave_flag) break;
    }

    close(cli->sockfd);
    remove_client(cli->uid);
    free(cli);
    pthread_detach(pthread_self());
    return NULL;
}

// Función para manejar paquetes TCP forjados usando raw sockets
void *handle_fake_clients(void *arg) {
    int raw_sock;
    char buffer[BUFFER_SIZE];

    struct sockaddr_in source;
    socklen_t addr_len = sizeof(source);

    // Crear un raw socket
    raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_sock < 0) {
        perror("Error creando socket raw");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Recibir paquetes crudos
        int packet_len = recvfrom(raw_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&source, &addr_len);
        if (packet_len < 0) {
            perror("Error recibiendo paquete");
            continue;
        }

        struct iphdr *ip_header = (struct iphdr *)buffer;
        struct tcphdr *tcp_header = (struct tcphdr *)(buffer + ip_header->ihl * 4); //tcphdr tiene dentro de sí la información del encabezado TCP
        //esta es: struct tcphdr { u_int16_t source; u_int16_t dest; u_int32_t seq; u_int32_t ack_seq; u_int16_t res1:4; 
        //u_int16_t doff:4; u_int16_t fin:1; u_int16_t syn:1; u_int16_t rst:1; u_int16_t psh:1; u_int16_t ack:1; u_int16_t urg:1;
        // u_int16_t res2:2; u_int16_t window; u_int16_t check; u_int16_t urg_ptr; };

        // Obtener IP y puerto de origen
        char source_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &source.sin_addr, source_ip, sizeof(source_ip));
        
        // Verificar si el puerto de destino es el puerto esperado (PORT)
        if (ntohs(tcp_header->dest) == PORT) { //tcp_header es un puntero a la estructura tcphdr que guarda la información del encabezado TCP 
            // Calcular el tamaño de los encabezados
            int ip_header_len = ip_header->ihl * 4;
            int tcp_header_len = tcp_header->doff * 4;

            // Calcular la posición del payload
            char *data = buffer + ip_header_len + tcp_header_len;
            int data_len = packet_len - (ip_header_len + tcp_header_len);

            // Si hay datos, mostrar el mensaje recibido
            if (data_len > 0) {
                printf("Mensaje desde %s: %.*s\n", source_ip, data_len, data);
            }


        }
    }

    close(raw_sock);
    return NULL;
}

int main() {
    int sockfd, new_sock;
    struct sockaddr_in server_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    pthread_t tid, fake_tid;

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

    printf("=== El servidor está corriendo ===\n");

    // Crear hilo para escuchar clientes falsos
    pthread_create(&fake_tid, NULL, &handle_fake_clients, NULL);

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