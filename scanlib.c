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
