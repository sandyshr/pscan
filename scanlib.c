#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <ip address or subnet mask> <start port> <end port>\n", argv[0]);
        exit(1);
    }

    char *host = argv[1];
    int start_port = atoi(argv[2]);
    int end_port = atoi(argv[3]);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    for (int port = start_port; port <= end_port; port++) {
        char port_str[6];
        sprintf(port_str, "%d", port);

        struct addrinfo *res;
        int result = getaddrinfo(host, port_str, &hints, &res);
        if (result != 0) {
            fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(result));
            continue;
        }

        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket() error");
            freeaddrinfo(res);
            continue;
        }

        // Set socket to non-blocking mode
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        // Initiate connection attempt
        int c = connect(sockfd, res->ai_addr, res->ai_addrlen);
        if (c < 0 ){
            switch (errno) {
                case EINPROGRESS:
                    fd_set write_fds;
                    FD_ZERO(&write_fds);
                    FD_SET(sockfd, &write_fds);
                    struct timeval timeout = {1, 0}; // Wait for 1 second
                    int select_result = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
                    if (select_result < 0) {
                        perror("select() error");
                    } else if (select_result == 0) {
                        // Connection attempt timed out, port is most likely filtered
                        //printf("Port %d is most likely filtered\n", port);
                    } else {
                        // Check if connection was successful
                        int optval;
                        socklen_t optlen = sizeof(optval);
                        int getsockopt_result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
                        if (getsockopt_result < 0) {
                            perror("getsockopt() error");
                        } else if (optval == 0) {
                            printf("Port %d is open\n", port);
                        }
                    }
                    break;
                case ECONNREFUSED:
                    // Connection refused, port is closed
                    break;
                default:
                    // Other error,

            }
        } else {
            printf("Port %d is open\n", port);
        }

       
    close(sockfd);
    freeaddrinfo(res);
    }
   

    return 0;
}


/*
version 2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>

#define MAX_PORTS 65535
#define BUFFER_SIZE 1024

void usage() {
    printf("Usage: ./openports -p <port range> <ip address or cidr notation>\n");
    printf("Example: ./openports -p 1-1024 192.168.0.1\n");
    printf("Example: ./openports -p 80 192.168.0.0/24\n");
}

void scan_ports(const char* ip_address, int start_port, int end_port) {
    
    struct addrinfo hints, *res, *p;
    int sockfd;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip_address, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Error in resolving IP address\n");
        return;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue; //continue to the next ip
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) { //setting non blocking flag
            perror("fcntl");
            close(sockfd);
            continue;
        }

        for (int port = start_port; port <= end_port; port++) {
            if (p->ai_family == AF_INET) {
                ((struct sockaddr_in *) p->ai_addr)->sin_port = htons(port);
            } else if (p->ai_family == AF_INET6) {
                ((struct sockaddr_in6 *) p->ai_addr)->sin6_port = htons(port);
            } else {
                fprintf(stderr, "Unknown address family\n");
                continue;
            }

            int c = connect(sockfd, p->ai_addr, p->ai_addrlen);
            if (c < 0 ){
                switch (errno) {
                    case EINPROGRESS:
                        fd_set write_fds;
                        FD_ZERO(&write_fds);
                        FD_SET(sockfd, &write_fds);
                        struct timeval timeout = {1, 0}; // Wait for 1 second
                        int select_result = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
                        if (select_result < 0) {
                            perror("select() error");
                        } else if (select_result == 0) {
                            // Connection attempt timed out, port is most likely filtered
                            printf("Port %d is most likely filtered\n", port);
                        } else {
                            // Check if connection was successful
                            int optval;
                            socklen_t optlen = sizeof(optval);
                            int getsockopt_result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
                            if (getsockopt_result < 0) {
                                perror("getsockopt() error");
                            } else if (optval == 0) {
                                printf("Port %d is open\n", port);
                            }else {
                                printf("Port %d is closed\n", port);
                            }
                        }
                        break;
                    case ECONNREFUSED:
                        // Connection refused, port is closed
                        break;
                    default:
                        // Other error,
                        break;
                }
            } else {
                printf("Port %d is open\n", port);
            }
        }

        close(sockfd);
    }

    freeaddrinfo(res);
}




int main(int argc, char** argv) {
    if (argc != 4) {
        usage();
        return 0;
    }

    if (strcmp(argv[1], "-p") != 0) {
        usage();
        return 0;
    }

    char* port_range = argv[2];
    char* ip_address = argv[3];

    int start_port, end_port;
    char* token = strtok(port_range, "-");
    start_port = atoi(token);
    if (start_port <= 0 || start_port > MAX_PORTS) {
        printf("Invalid start port specified\n");
        return 0;
    }

    token = strtok(NULL, "-");
    if (token == NULL) {
        end_port = start_port;
    } else {
        end_port = atoi(token);
        if (end_port <= 0 || end_port > MAX_PORTS) {
            printf("Invalid end port specified\n");
            return 0;
        }
    }

    if (end_port < start_port) {
        printf("Invalid port range specified\n");
        return 0;
    }

    if (inet_addr(ip_address) == -1) {
        printf("Invalid IP address specified\n");
        return 0;
    }


// Check if the IP address is in CIDR notation
char* slash_pos = strchr(ip_address, '/');
if (slash_pos != NULL) {
    int prefix_length = atoi(slash_pos + 1);
    if (prefix_length <= 0 || prefix_length > 32) {
        printf("Invalid CIDR prefix length specified\n");
        return 0;
    }

    char ip_address_copy[strlen(ip_address) + 1];
    strcpy(ip_address_copy, ip_address);
    char* dot_pos = strrchr(ip_address_copy, '.');
    if (dot_pos == NULL) {
        printf("Invalid IP address specified\n");
        return 0;
    }

    *dot_pos = '\0';
    char* network_address_str = ip_address_copy;
    char* host_address_str = dot_pos + 1;

    struct in_addr network_address;
    if (inet_pton(AF_INET, network_address_str, &network_address) != 1) {
        printf("Invalid network address specified\n");
        return 0;
    }

    uint32_t network_address_int = ntohl(network_address.s_addr);
    uint32_t host_address_min = network_address_int | ((1 << (32 - prefix_length)) - 1);
    uint32_t host_address_max = network_address_int | (~0 << (32 - prefix_length));

    for (uint32_t host_address_int = host_address_min; host_address_int <= host_address_max; host_address_int++) {
        struct in_addr host_address;
        host_address.s_addr = htonl(host_address_int);
        scan_ports(inet_ntoa(host_address), start_port, end_port);
    }
} else {
    // Single IP address
    scan_ports(ip_address, start_port, end_port);
}

return 0;
}
*/
