#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_LEN ((size_t) 480)

int main(int argc, char **argv) {
	char *server_name;
	char *port_name;
	struct addrinfo hints;
	int gai_code;
	struct addrinfo *result;
	struct addrinfo *curr;
	int found;
	int fd;
	ssize_t read_res;
	size_t bytes_to_be_sent;
	char buf[480];

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
	if (gai_code != 0) {
		fprintf(stderr, "Could not look up server address info for %s %s\n",
			server_name, port_name);
		return 1;
	}

	/* Iterate over the linked list returned by getaddrinfo */
	found = 0;
	for (curr=result; curr !=NULL; curr=curr->ai_next) {
		fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (fd >= 0) {
			if (connect(fd, curr->ai_addr, curr->ai_addrlen) >= 0) {
				found = 1;
				break;
			} else {
				if (close(fd) < 0) {
					fprintf(stderr, "Could not close a socket: %s\n",
						strerror(errno));
					freeaddrinfo(result);
					return 1;
				}
			}
		}
	}
	freeaddrinfo(result);
	if (!found) {
		fprintf(stderr, "Could not connect to any possible results for %s %s \n",
			server_name, port_name);
		return 1;
	}

	/* Now we need to read stdin and send UDP packets is chunks of 
	at most 480 bytes.
	*/
	for (;;) {
		read_res = read(0, buf, BUFFER_LEN);
		if (read_res == ((ssize_t) 0)) {
			/* We have hit EOF send UDP packet of size 0 to peer */
			bytes_to_be_sent = (size_t) 0;
			if (send(fd, buf, bytes_to_be_sent, 0) < ((ssize_t) 0)) {
				fprintf(stderr, "Error with send: %s\n",
					strerror(errno));
				return 1;
			}
			break;
		}
		if (read_res < ((ssize_t) 0)) {
			/* Error on read() */
			fprintf(stderr, "Error using read: %s\n",
				strerror(errno));
			return 1;
		}
		bytes_to_be_sent = (size_t)read_res;
		if (send(fd, buf, bytes_to_be_sent, 0) < ((ssize_t) 0)) {
			fprintf(stderr, "Error with send: %s\n",
				strerror(errno));
			return 1;
		}
	}
	// TODO Close fd
	return 0;
}
