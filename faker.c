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

#define PACKET_SIZE 4096 // Tamaño del paquete
#define MTU_SIZE 1500 // Tamaño máximo de transmisión para Ethernet
#define ARPHRD_ETHER 1 // Ethernet

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void send_tcp_packet(const char *src_ip, const char *dst_ip, int src_port, int dst_port, const char *data) {
    int sockfd;
    char packet[PACKET_SIZE];

    // Crear socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Limpiar el paquete
    memset(packet, 0, PACKET_SIZE);

    // Encabezado Ethernet
    struct ethhdr *eth_header = (struct ethhdr *)packet;
    memcpy(eth_header->h_dest, "\xA0\x36\xBC\xAA\x8F\xD8", ETH_ALEN); // Dirección MAC de destino
    memcpy(eth_header->h_source, "\xA0\x36\xBC\xAA\x8D\xA5", ETH_ALEN); // Dirección MAC de origen
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
        fprintf(stderr, "Error: Data size exceeds MTU limit. Reduce the size of the data.\n");
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
    tcp_header->seq = 0; // Número de secuencia
    tcp_header->ack_seq = 0; // Número de reconocimiento
    tcp_header->doff = 5; // Longitud del encabezado TCP
    tcp_header->fin = 0; // Bandera FIN
    tcp_header->syn = 1; // Bandera SYN
    tcp_header->rst = 0; // Bandera RST
    tcp_header->psh = 0; // Bandera PSH
    tcp_header->ack = 0; // Bandera ACK
    tcp_header->urg = 0; // Bandera URG
    tcp_header->window = htons(5840); // Tamaño de la ventana
    tcp_header->check = 0; // Inicialmente 0
    tcp_header->urg_ptr = 0; // Puntero urgente

    // Calcular el checksum del encabezado TCP
    tcp_header->check = checksum((unsigned short *)tcp_header, sizeof(struct tcphdr) + data_length);

    // Configurar la dirección del destino
    struct sockaddr_ll sa;
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
        perror("Error sending packet");
        exit(EXIT_FAILURE);
    }

    // Cerrar el socket
    close(sockfd);
    printf("Paquete enviado: %s\n", data);
}

int main(){
    const char *src_ip = "10.10.110.143"; // Dirección IP de origen****************
    const char *dst_ip = "10.10.110.141"; // Dirección IP de destino****************
    int src_port = 46328; // Puerto de origen
    int dst_port = 8080; // Puerto de destino********

    
    const char *data = "Hola soy"; // Mensaje a enviar

    send_tcp_packet(src_ip, dst_ip, src_port, dst_port, data);
    return 0;
}

