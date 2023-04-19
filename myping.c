#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <string.h>

#define PACKET_SIZE 64

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char*)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;

    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <IP address>\n", argv[0]);
        return 1;
    }

    int sockfd;
    char packet[PACKET_SIZE];
    struct sockaddr_in addr;
    struct icmphdr *icmp;
    int sent, received;
    struct timeval timeout = { 1, 0 }; // 1 second

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    memset(packet, 0, sizeof(packet));
    icmp = (struct icmphdr*)packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = getpid();
    icmp->un.echo.sequence = 0;
    icmp->checksum = checksum(packet, sizeof(struct icmphdr));

    sent = sendto(sockfd, packet, sizeof(struct icmphdr), 0, (struct sockaddr*)&addr, sizeof(addr));
    if (sent < 0) {
        perror("sendto");
        return 1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    received = recvfrom(sockfd, packet, sizeof(packet), 0, NULL, NULL);
    if (received < 0) {
        printf("%s is down\n", argv[1]);
        return 1;
    }

    printf("%s is up\n", argv[1]);

    return 0;
}
