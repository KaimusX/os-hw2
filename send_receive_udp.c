#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

#define READ_BUFFER_SIZE 480
#define RECV_BUFFER_SIZE 65536  // 2^16 bytes

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

ssize_t better_write(int fd, const char *buf, size_t count) {
  size_t already_written, to_be_written, written_this_time, max_count;
  ssize_t res_write;

  if (count == ((size_t) 0)) return (ssize_t) count;
  
  already_written = (size_t) 0;
  to_be_written = count;
  while (to_be_written > ((size_t) 0)) {
    max_count = to_be_written;
    if (max_count > ((size_t) 8192)) {
      max_count = (size_t) 8192;
    }
    res_write = write(fd, &(((const char *) buf)[already_written]), max_count);
    if (res_write < ((size_t) 0)) {
      /* Error */
      return res_write;
    }
    if (res_write == ((ssize_t) 0)) {
      /* Nothing written, stop trying */
      return (ssize_t) already_written;
    }
    written_this_time = (size_t) res_write;
    already_written += written_this_time;
    to_be_written -= written_this_time;
  }
  return (ssize_t) already_written;
}

int main(int argc, char *argv[]) {
    const char *server_name;
    const char *port_name;
    uint16_t port;
    struct addrinfo hints;
    int gai_code;
	struct addrinfo *result;
    struct addrinfo *curr;
    int found;
	int sockfd;
    struct sockaddr *server_addr = NULL;
    socklen_t server_addr_len;
    char read_buffer[READ_BUFFER_SIZE];
    char recv_buffer[RECV_BUFFER_SIZE];
    

    if (argc < 3) {
        fprintf(stderr, "Not enough arguments: %s <server name> <port name>\n", ((argc > 0) ? argv[0] : "./send_receive_udp")); //Could port or server name be provided in any order??
		return 1;
    }

    server_name = argv[1];
    port_name = argv[2];

    if (convert_port_name(&port, port_name) < 0) {
        fprintf(stderr, "Error with convert_port_name\n");
        return 1;
    }

    // Resolve server address using getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
	hints.ai_flags = 0;

	gai_code = getaddrinfo(server_name, port_name, &hints, &result);
	if (gai_code != 0) {
		fprintf(stderr, "Could not look up server address info for %s %s\n", server_name, port_name);
		return 1;
	}

    /* Iterate over the linked list returned by getaddrinfo */
	found = 0;
	for (curr=result; curr !=NULL; curr=curr->ai_next) {
		sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (sockfd >= 0) {
			if (connect(sockfd, curr->ai_addr, curr->ai_addrlen) >= 0) {
                server_addr = curr->ai_addr;
                server_addr_len = curr->ai_addrlen;
				found = 1;
				break;
			} else {
				if (close(sockfd) < 0) {
					fprintf(stderr, "Could not close a socket: %s\n", strerror(errno));
					freeaddrinfo(result); 
					return 1;
				}
			}
		}
	}
	freeaddrinfo(result); 
	if (!found) {
		fprintf(stderr, "Could not connect to any possible results for %s %s \n", server_name, port_name);
		return 1;
	}

    // Set socket to non-blocking mode
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    fcntl(0, F_SETFL, O_NONBLOCK);

    // Main loop
    for (;;) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);
        FD_SET(sockfd, &read_fds);
        ssize_t sent_len;

        // Use select to wait for activity on either stdin or the socket
        int nfds = (sockfd > 0 ? sockfd + 1 : 1);
        int select_result = select(nfds, &read_fds, NULL, NULL, NULL);
        if (select_result < 0) {
            perror("select");
            fprintf(stderr, "Error with select: %s", strerror(errno));
            if (close(sockfd) < 0) {
                fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
			}
            freeaddrinfo(result); 
            return 1;
        }

        // Check if data is available to read from standard input
        if (FD_ISSET(0, &read_fds)) {
            ssize_t read_len = read(0, read_buffer, READ_BUFFER_SIZE);
            if (read_len < 0) {
                fprintf(stderr, "Error with read: %s", strerror(errno));
                break;
            } else if (read_len == 0) {
                // EOF on stdin, terminate
                break;
            } else {
                // Send the read data as a UDP packet to the server
                sent_len = sendto(sockfd, read_buffer, read_len, 0, server_addr, server_addr_len);
                if (sent_len < 0) {
                    fprintf(stderr, "Error with sendto: %s", strerror(errno));
                    break;
                }
            }
        }

        // Check if data is available to read from the socket
        if (FD_ISSET(sockfd, &read_fds)) {
            ssize_t recv_len = recv(sockfd, recv_buffer, RECV_BUFFER_SIZE, 0);
            if (recv_len < 0) {
                fprinf(stderr, "Error with recv: %s", strerror(errno));
                break;
            } else if (recv_len == 0) {
                // Received an empty UDP packet, terminate
                break;
            } else {
                // Write received data to standard output
                if (better_write(1, recv_buffer, recv_len) < 0) {
                    fprintf(stderr, "Error with better_write\n");
                    break;
                }
            }
        }
    }

    // Clean up
    if (close(sockfd) < 0) {
        fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
    }
    freeaddrinfo(result); 
    return 0;
}
