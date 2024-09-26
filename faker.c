#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <errno.h>

#define PACKET_SIZE 4096
#define MTU_SIZE 1500 // Tamaño máximo de transmisión para Ethernet
#define ARPHRD_ETHER 1 // Ethernet es un tipo de red local

// Función para calcular el checksum, la checksum es la suma de verificación de los datos
//es decir, nos sirve para verificar la integridad de los datos que se envían en un paquete de red 
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++; // Sumar el valor del puntero buf a sum y luego incrementar el puntero de 2 en 2
    }
    if (len == 1) {
        sum += *(unsigned char *)buf; 
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Estructura para el pseudo encabezado TCP
struct pseudo_header {
    u_int32_t source_address; // Dirección IP de origen
    u_int32_t dest_address; // Dirección IP de destino
    u_int8_t placeholder; // Relleno, es decir, un byte de relleno para el protocolo 
    u_int8_t protocol; // Protocolo
    u_int16_t tcp_length; // Longitud del encabezado TCP
};

// Función para calcular el checksum TCP
unsigned short tcp_checksum(struct tcphdr *tcp_header, const char *data, size_t data_length, const char *src_ip, const char *dst_ip) {
    //los argumentos de esta función son el encabezado TCP, los datos, la longitud de los datos, la dirección IP de origen y la dirección IP de destino
    
    struct pseudo_header psh; // Pseudo encabezado
    psh.source_address = inet_addr(src_ip);
    psh.dest_address = inet_addr(dst_ip);
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + data_length);

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + data_length;
    char *pseudogram = malloc(psize);

    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcp_header, sizeof(struct tcphdr));
    memcpy(pseudogram + sizeof(struct pseudo_header) + sizeof(struct tcphdr), data, data_length);

    unsigned short checksum_value = checksum((unsigned short *)pseudogram, psize);
    free(pseudogram);
    return checksum_value;
}

void send_tcp_packet(const char *src_ip, const char *dst_ip, int src_port, int dst_port, const char *data, unsigned int seq_num, unsigned int ack_num) {
    int sockfd;
    char packet[PACKET_SIZE];
    struct sockaddr_ll sa;

    // Crear socket RAW para enviar y recibir
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("Error creando socket");
        exit(EXIT_FAILURE);
    }

    // Limpiar el paquete
    memset(packet, 0, PACKET_SIZE);

    // Encabezado Ethernet
    struct ethhdr *eth_header = (struct ethhdr *)packet;
    memcpy(eth_header->h_dest, "\xA0\x36\xBC\xAA\x8D\xA5", ETH_ALEN); // Dirección MAC de destino
    memcpy(eth_header->h_source,"\xA0\x36\xBC\xAA\x8F\xD8" , ETH_ALEN); // Dirección MAC de origen
    eth_header->h_proto = htons(ETH_P_IP); // Tipo de protocolo

    // Encabezado IP
    struct iphdr *ip_header = (struct iphdr *)(packet + sizeof(struct ethhdr));
    ip_header->version = 4; // IPv4
    ip_header->ihl = 5; // Longitud del encabezado
    ip_header->ttl = 64; // Tiempo de vida
    ip_header->protocol = IPPROTO_TCP; // Protocolo TCP
    ip_header->saddr = inet_addr(src_ip); // Dirección IP de origen
    ip_header->daddr = inet_addr(dst_ip); // Dirección IP de destino
  
    // Copiar los datos en el paquete
    size_t data_length = strlen(data);
    if (data_length + sizeof(struct iphdr) + sizeof(struct tcphdr) > MTU_SIZE - sizeof(struct ethhdr)) {
        fprintf(stderr, "Error: El tamaño de los datos excede el límite MTU. Reduce el tamaño de los datos.\n");
        close(sockfd);
        return;
    }
    memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct tcphdr), data, data_length);

    // Calcular la longitud total del paquete
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + data_length);
   
    // Calcular el checksum del encabezado IP
    ip_header->check = checksum((unsigned short *)ip_header, sizeof(struct iphdr));

    // Encabezado TCP
    struct tcphdr *tcp_header = (struct tcphdr *)(packet + sizeof(struct ethhdr) + sizeof(struct iphdr));
    tcp_header->source = htons(src_port); // Puerto de origen
    tcp_header->dest = htons(dst_port); // Puerto de destino
    tcp_header->seq = htonl(seq_num); // Número de secuencia
    tcp_header->ack_seq = htonl(ack_num); // Número de reconocimiento
    tcp_header->doff = 5; // Longitud del encabezado TCP
    tcp_header->fin = 0; // Bandera FIN
    tcp_header->syn = 0; // Bandera SYN
    tcp_header->rst = 0; // Bandera RST
    tcp_header->psh = 1; // Bandera PSH
    tcp_header->ack = 1; // Bandera ACK
    tcp_header->urg = 0; // Bandera URG
    tcp_header->window = htons(502); // Tamaño de la ventana
    tcp_header->check = 0; // Inicialmente 0

    // Calcular el checksum del encabezado TCP correctamente
    tcp_header->check = tcp_checksum(tcp_header, data, data_length, src_ip, dst_ip);

    // Configurar la dirección del destino
    memset(&sa, 0, sizeof(struct sockaddr_ll));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_IP);
    sa.sll_hatype = ARPHRD_ETHER; // Tipo de ARP para Ethernet
    sa.sll_pkttype = PACKET_OTHERHOST; // Tipo de paquete
    sa.sll_halen = ETH_ALEN;
    memcpy(sa.sll_addr, eth_header->h_dest, ETH_ALEN);
    sa.sll_ifindex = 2; // Índice de la interfaz, puede ser diferente

    // Enviar el paquete
    if (sendto(sockfd, packet, ntohs(ip_header->tot_len) + sizeof(struct ethhdr), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("Error enviando el paquete");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Paquete enviado: %s\n", data);

    // Recibir el paquete ACK
    char recv_buffer[PACKET_SIZE];
    while (1) {
        ssize_t recv_len = recv(sockfd, recv_buffer, PACKET_SIZE, 0);
        if (recv_len < 0) {
            perror("Error recibiendo datos");
            break;
        }

        // Analizar el paquete recibido
        struct iphdr *recv_ip_header = (struct iphdr *)(recv_buffer + sizeof(struct ethhdr));
        struct tcphdr *recv_tcp_header = (struct tcphdr *)(recv_buffer + sizeof(struct ethhdr) + sizeof(struct iphdr));

        if (recv_ip_header->protocol == IPPROTO_TCP && recv_tcp_header->ack) {
            printf("ACK recibido. Número de secuencia: %u\n", ntohl(recv_tcp_header->ack_seq));
            break;
        }
    }

    // Cerrar el socket
    close(sockfd);
}

int main() {
    const char *src_ip = "10.10.110.143"; // Dirección IP de origen
    const char *dst_ip = "10.10.110.141"; // Dirección IP de destino
    int src_port = 37196; // Puerto de origen
    int dst_port = 8080; // Puerto de destino
    const char *data = "hola"; // Mensaje a enviar

    unsigned int seq_num = 1827459732 + 4; // Número de secuencia para el segundo mensaje
    unsigned int ack_num = 1401105231; // Número de reconocimiento del primer mensaje

    send_tcp_packet(src_ip, dst_ip, src_port, dst_port, data, seq_num, ack_num);
    return 0;
}
