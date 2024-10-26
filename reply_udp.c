#include <stdio.h>

int main(int argc, char **argv) {
	char *port_name;
	/* Get port number as argument */
	if (argc != 2) {
		fprintf(stderr, "Not enough arguments: %s <port name>\n", ((argc > 0)
			? argv[0] : "./reply_udp"));
	}
	port_name = argv[1];

	return 0;
}
