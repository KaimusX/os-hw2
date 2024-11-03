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
#include <netinet/in.h>

#define BUFFER_SIZE 65536  // 2^16 bytes

// Function to convert a port name to a 16-bit unsigned integer
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

// Better write
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
    const char *tcp_port_name;
    const char *udp_server_name;
    const char *udp_port_name;
    uint16_t tcp_port;
    int tcp_sockfd;
    struct sockaddr_in addr;
    struct addrinfo hints;
	struct addrinfo *result;
    int gai_code;
    struct addrinfo *curr;
    int found;
    int udp_sockfd = 0; 
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int tcp_client_sockfd;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_received;

    // Check for arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <TCP port> <UDP server name> <UDP port>\n", ((argc > 0) ? argv[0] : "./tunnel_udp_over_tcp_server"));
        return 1;
    }

    // Assign variables
    tcp_port_name = argv[1];
    udp_server_name = argv[2];
    udp_port_name = argv[3];

    // Get port number
    if (convert_port_name(&tcp_port, tcp_port_name) < 0) {
        fprintf(stderr, "Error with convert_port_name: %s\n", strerror(errno));
        return 1;
    }

    // Create socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // For if socket failed
    if (tcp_sockfd < 0) {
        fprintf(stderr, "Error with scoket: %s\n", strerror(errno));
        return 1; 
    }

    // Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcp_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // Binding
    if (bind(tcp_sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Could not bind a TCP socket: %s\n", strerror(errno));
        if (close(tcp_sockfd) < 0) {
            fprintf(stderr, "Could not close a TCP socket: %s\n", strerror(errno));      
        }
        return 1;
    }

    // Get address info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
	hints.ai_flags = 0;

	gai_code = getaddrinfo(udp_server_name, udp_port_name, &hints, &result);
	if (gai_code != 0) {
		fprintf(stderr, "Could not look up server address info for %s %s\n", udp_server_name, udp_port_name);
        if (close(tcp_sockfd) < 0) {
            fprintf(stderr, "Could not close a TCP socket: %s\n", strerror(errno));      
        }
        return 1;
	}

    // Create socket and check connect 
	found = 0;
	for (curr=result; curr !=NULL; curr=curr->ai_next) {
		udp_sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (udp_sockfd >= 0) {
			if (connect(udp_sockfd, curr->ai_addr, curr->ai_addrlen) >= 0) {
				found = 1;
				break;
			} else {
				if (close(tcp_sockfd) < 0) {
					fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
				}
                if (close(udp_sockfd) < 0) {
                    fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
                }
                freeaddrinfo(result); 
				return 1;
			}
		}
	}
    // For if connect failed
	if (!found) {
		fprintf(stderr, "Could not connect to any possible results for %s %s \n", udp_server_name, udp_port_name);
        if (close(tcp_sockfd) < 0) {
            fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
        }
        if (close(udp_sockfd) < 0) {
            fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
        }
        freeaddrinfo(result); 
		return 1;
	}

    // Free address info
    freeaddrinfo(result); 

    // Listen on TCP
    if (listen(tcp_sockfd, 1) < 0) {
        fprintf(stderr, "Error with listen: %s\n", strerror(errno));
        if (close(tcp_sockfd) < 0) {
            fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
        }
        if (close(udp_sockfd) < 0) {
            fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
        }
        return 1;
    }

    // Wait for connections
    tcp_client_sockfd = accept(tcp_sockfd, (struct sockaddr *) &client_addr, &client_addr_len); 
    if(tcp_client_sockfd  < 0) {
        fprintf(stderr, "Error with accept: %s\n", strerror(errno));
        if (close(tcp_sockfd) < 0) {
            fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
        }
        if (close(udp_sockfd) < 0) {
            fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
        }
        return 1;
    }

    // Set socket to non-blocking mode
    fcntl(tcp_client_sockfd, F_SETFL, O_NONBLOCK);
    fcntl(udp_sockfd, F_SETFL, O_NONBLOCK);

    // Main loop
    for (;;) {
        // Clear FD set
        FD_ZERO(&read_fds);

        //Add FDs to set
        FD_SET(tcp_client_sockfd, &read_fds);
        FD_SET(udp_sockfd, &read_fds);

        // Select 
        int nfds = (tcp_client_sockfd > udp_sockfd ? tcp_client_sockfd : udp_sockfd);
        int select_result = select(nfds + 1, &read_fds, NULL, NULL, NULL);
        // For if select failed
        if (select_result < 0) {
            fprintf(stderr, "Error with select: %s\n", strerror(errno));
            if (close(tcp_client_sockfd) < 0) {
                fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
            }
            if (close(tcp_sockfd) < 0) {
                fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
            }
            if (close(udp_sockfd) < 0) {
                fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
            }
            return 1;
        }

        // Check for data on TCP client
        if (FD_ISSET(tcp_client_sockfd, &read_fds)) {
            // Read 
            bytes_read = read(tcp_client_sockfd, buffer, BUFFER_SIZE);
            // For if read failed
            if (bytes_read < 0) {
                fprintf(stderr, "Error with read: %s\n", strerror(errno));
                    if (close(tcp_client_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(tcp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(udp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
                    }
                return 1;
            } else if (bytes_read == 0) {
                // EOF
                break;
            } else {
                // Forward TCP data to the UDP server
                bytes_sent = send(udp_sockfd, buffer, bytes_read, 0);
                if (bytes_sent < 0) {
                    fprintf(stderr, "Error with send: %s\n", strerror(errno));
                    if (close(tcp_client_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(tcp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(udp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
                    }
                    return 1;
                }
            }
        }

        // Check for data on UDP socket
        if (FD_ISSET(udp_sockfd, &read_fds)) {
            // Recieved 
            bytes_received = recv(udp_sockfd, buffer, BUFFER_SIZE, 0);
            // For if received failed
            if (bytes_received < 0) {
                fprintf(stderr, "Error with recv: %s\n", strerror(errno));
                if (close(tcp_client_sockfd) < 0) {
                    fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                }
                if (close(tcp_sockfd) < 0) {
                    fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                }
                if (close(udp_sockfd) < 0) {
                    fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
                }
                return 1;
            } else if (bytes_received == 0) {
                // EOF
                break;
            } else {
                // Forward UDP data to the TCP client
                bytes_sent = better_write(tcp_client_sockfd, buffer, bytes_received);
                if (bytes_sent < 0) {
                    fprintf(stderr, "Error with better_write: %s\n", strerror(errno));
                    if (close(tcp_client_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(tcp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
                    }
                    if (close(udp_sockfd) < 0) {
                        fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
                    }
                    return 1;
                }
            }
        }
    }

    // Clean up
    if (close(tcp_client_sockfd) < 0) {
        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
        return 1; 
    }
    if (close(tcp_sockfd) < 0) {
        fprintf(stderr, "Could not close a tcp socket: %s\n", strerror(errno));
        return 1; 
    }
    if (close(udp_sockfd) < 0) {
        fprintf(stderr, "Could not close a udp socket: %s\n", strerror(errno));
        return 1; 
    }

    // Success
    return 0;
}
