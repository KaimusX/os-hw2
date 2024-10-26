#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

int main(int argc, char **argv) {
	char *port_name;
    uint16_t port;
	int sockfd;

	/* Get port number as argument */
	if (argc != 2) {
		fprintf(stderr, "Not enough arguments: %s <port name>\n", ((argc > 0)
			? argv[0] : "./reply_udp"));
	}
	port_name = argv[1];

	/* Convert port name to 16 bit unsigned integer. */
    if (convert_port_name(&port, port_name) < 0) {
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

	return 0;
}