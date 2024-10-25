#include <stdio.h>

int main(int argc, char **argv) {
	char *server_name;
	char *port_name;
	/* Checks arguments for server name and port name */
	if (argc < 3) {
		fprintf(stderr, "Not enough arguments.");
		return 1;
	}
	server_name = argv[1];
	port_name = argv[2];

	return 0;
}
