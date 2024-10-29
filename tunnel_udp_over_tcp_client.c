#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

int main (int argc, char **argv) {
    uint16_t udp_port;
	char *udp_port_name;
	char *tcp_server_name;
	char *tcp_port_name;
	int sockfd;
    struct sockaddr_in addr;

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
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Could not open a UDP socket: %s\n",
			strerror(errno));
		return 1;
	}
	
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(udp_port);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Could not bind a UDP socket: %s\n", strerror(errno));
        if (close(sockfd) < 0) {
            fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));      
        }
        return 1;
    }

	return 0;
}
