#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#define BUFFER_LEN ((size_t) 480)

int main(int argc, char **argv) {
	char *server_name;
	char *port_name;
	struct addrinfo hints;
	int gai_code;
	struct addrinfo *result;

	/* Checks arguments for server name and port name */
	if (argc < 3) {
		fprintf(stderr, "Not enough arguments.");
		return 1;
	}
	server_name = argv[1];
	port_name = argv[2];
	
	/* Use getaddrinfo() to get the right type of address information
	for the name and port we have got.
	*/
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;
	gai_code = getaddrinfo(server_name, port_name, &hints, &result);

	return 0;
}
