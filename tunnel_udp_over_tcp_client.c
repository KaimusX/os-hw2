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

// better_write credit to Dr. Cristoph Lauter
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

uint16_t get_msg_size(uint16_t recv_bytes, uint16_t buf_size) {
	return (recv_bytes < buf_size) ? recv_bytes : buf_size;
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
	char msg_buf[UDP_BUF_SIZE + 2]; // +2 for the header indicating message length.
	fd_set read_fds;
	int nfds = get_nfds(udp_sockfd, tcp_sockfd);
    struct sockaddr_in udp_addr;
	socklen_t udp_addr_len = sizeof(udp_addr);
	ssize_t recv_bytes;
	int udp_info_ready = -1;
	uint16_t msg_size;
	uint16_t udp_packet_size_header;

	for(;;) {
		/* Set FD's for select */
		FD_ZERO(&read_fds);
		FD_SET(udp_sockfd, &read_fds);
		FD_SET(tcp_sockfd, &read_fds);
		
		/* Try to select */
		if (select(nfds, &read_fds, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "Could not select: %s\n",
				strerror(errno));
			if (close(udp_sockfd) < 0) {
				fprintf(stderr, "Could not close UDP sockfd: %s\n",
					strerror(errno));
			}
			if (close(tcp_sockfd) < 0) {
				fprintf(stderr, "Could not close TCP sockfd: %s\n",
					strerror(errno));
			}
			return 1;
		}

		/* UDP socket ready, recvfrom packets. */
		if (FD_ISSET(udp_sockfd, &read_fds)) {
			recv_bytes = recvfrom(udp_sockfd, udp_buf, UDP_BUF_SIZE, 0, 
			(struct sockaddr *) &udp_addr, &udp_addr_len);
			if (recv_bytes < (ssize_t) 0) {
				fprintf(stderr, "Could not recvfrom on UDP socket %s\n",
					strerror(errno));
				return 1;
			}
			/* Bytes received, udp info is ready */
			udp_info_ready = 0;

			/* Create message of size at most 2^16 + 2 bytes. */
			msg_size = get_msg_size((uint16_t) recv_bytes, (uint16_t) UDP_BUF_SIZE);
			/* Store udp packet size first */
			udp_packet_size_header = htons(msg_size);
			msg_buf[0] = (udp_packet_size_header >> 8) & 0xFF; // Upper 8 bytes
			msg_buf[1] = udp_packet_size_header & 0xFF; // Lower 8 bytes
			
			/* Now we can put msg after header */
			if (msg_size > 0) {
				for (int i = 0; i < msg_size; i++) {
					msg_buf[i + 2] = udp_buf[i]; // Copy after 2 header bytes 
				}
			}

			/* Write formatted message to TCP */
			ssize_t bytes_to_be_written = msg_size + 2;
			if (better_write(tcp_sockfd, msg_buf, bytes_to_be_written) < 0){
				fprintf(stderr, "Could not  write to TCP socket: %s\n",
					strerror(errno));
				if (close(udp_sockfd) < 0) {
					fprintf(stderr, "Could not close UDP socket: %s\n",
						strerror(errno));
					if (close(tcp_sockfd) < 0) {
						fprintf(stderr, "Could not close a TCP socket: %s\n",
							strerror(errno));
					}
				}
				return 1;
			}
		}


	}
	
	return 0;
}
