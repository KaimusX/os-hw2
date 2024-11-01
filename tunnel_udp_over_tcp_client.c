#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define UDP_BUF_SIZE (1<<16)
#define TCP_BUF_SIZE ((1<<16) + 2)
#define RECONSTR_BUF_SIZE ((1<<17) +4)

// Convert port name to a 16-bit unsigned integer
static int convert_port_name(uint16_t *port, const char *port_name) {
    char *end;
    long long int nn;
    uint16_t t;
    long long int tt;

    if (port_name == NULL) return -1;
    if (*port_name == '\0') return -1;
    nn = strtoll(port_name, &end, 0);
    if (*end != '\0') return -1;
    if (nn < ((long long int) 0)) return -1;
    t = (uint16_t) nn;
    tt = (long long int) t;
    if (tt != nn) return -1;
    *port = t;
    return 0;
}

int get_nfds(int fd1, int fd2) {
	return (fd1 > fd2) ? fd1 + 1 : fd2 + 1;
}

int main (int argc, char **argv) {
    uint16_t udp_port;
	char *udp_port_name;
	char *tcp_server_name;
	char *tcp_port_name;
    struct sockaddr_in addr;
	struct addrinfo hints;
	int gai_code;
	struct addrinfo *result;
	struct addrinfo *curr;
	int found;
	int udp_sockfd;
	int tcp_sockfd;
	
	/* Checks arguments for udp port, tcp server name, tcp port name. */
	if (argc != 4) {
		fprintf(stderr, "Not enough arguments: %s <udp_port_name> <tcp_server_name> <tcp_port_name>\n",
			((argc > 0) ? argv[0] : "./tunnel_udp_over_tcp_client"));
		return 1;
	}	
	udp_port_name = argv[1];
	tcp_server_name = argv[2];
	tcp_port_name = argv[3];
	
	/* Convert port name to 16 bit unsigned integer. Save in udp_port variable. */
    if (convert_port_name(&udp_port, udp_port_name) < 0) {
        fprintf(stderr, "Error with convert_port_name\n");
        return 1;
    }
	
	/* Open a UDP socket at port */
	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (udp_sockfd < 0) {
		fprintf(stderr, "Could not open a UDP socket: %s\n",
			strerror(errno));
		return 1;
	}
	
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(udp_port);

    if (bind(udp_sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Could not bind a UDP socket: %s\n", strerror(errno));
        if (close(udp_sockfd) < 0) {
            fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));      
        }
        return 1;
    }
	
	/* Open a TCP connection to TCP server name and TCP port. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;
	result = NULL;
	gai_code = getaddrinfo(tcp_server_name, tcp_port_name, &hints, &result);

	if (gai_code != 0) {
		fprintf(stderr, "Could not look up server address info for %s %s\n",
			tcp_server_name, tcp_port_name);
		return 1;
	}
	/* Iterate over the linked list returned by getaddrinfo */
	found = 0;
	for (curr = result; curr != NULL; curr=curr->ai_next) {
		tcp_sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (tcp_sockfd >= 0) {
			if (connect(tcp_sockfd, curr->ai_addr, curr->ai_addrlen) >= 0) {
				found = 1;
				break;
			} else {
				if (close(tcp_sockfd) < 0) {
					fprintf(stderr, "Could not close a TCP socket: %s\n",
						strerror(errno));
					freeaddrinfo(result);
					return 1;
				}
			}
		}
	}
	freeaddrinfo(result);
	if (!found) {
		fprintf(stderr, "Could not connect to any possible results for %s %s\n",
			tcp_server_name, tcp_port_name);
		return 1;
	}

	/* We must use select for packets on either socket */	
	char udp_buf[UDP_BUF_SIZE];
	char tcp_buf[TCP_BUF_SIZE];
	char reconstr_buf[RECONSTR_BUF_SIZE];
	fd_set read_fds;
	int nfds = get_nfds(udp_sockfd, tcp_sockfd);
	
	return 0;
}
