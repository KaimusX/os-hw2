#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>

#define SEND_BUFFER_SIZE 480
#define RECV_BUFFER_SIZE 65536  // 2^16 bytes

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
    const char *server_name;
    const char *port_name;
    struct addrinfo hints;
	struct addrinfo *result;
    int gai_code;
    struct addrinfo *curr;
    int found;
	int sockfd;
    char send_buffer[SEND_BUFFER_SIZE];
    char recv_buffer[RECV_BUFFER_SIZE];
    fd_set read_fds;
    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_received;
    
    // Check for arguments
    if (argc < 3) {
        fprintf(stderr, "Not enough arguments: %s <server name> <port name>\n", ((argc > 0) ? argv[0] : "./send_receive_udp")); 
		return 1;
    }

    // Assign variables
    server_name = argv[1];
    port_name = argv[2];

    // Get address info
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

    // Create socket and check connect 
	found = 0;
	for (curr=result; curr !=NULL; curr=curr->ai_next) {
		sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (sockfd >= 0) {
			if (connect(sockfd, curr->ai_addr, curr->ai_addrlen) >= 0) {
				found = 1;
				break;
			} else {
				if (close(sockfd) < 0) {
					fprintf(stderr, "Could not close a socket: %s\n", strerror(errno));
				}
                freeaddrinfo(result); 
				return 1;
			}
		}
	}
    // For if connect failed
	if (!found) {
		fprintf(stderr, "Could not connect to any possible results for %s %s \n", server_name, port_name);
		return 1;
	}

    // Free address info
    freeaddrinfo(result); 

    // Set socket to non-blocking mode
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    // Main loop
    for (;;) {
        // Clear FD set
        FD_ZERO(&read_fds);
        
        //Add FDs to set
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        // Select
        int nfds = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
        int select_result = select(nfds + 1, &read_fds, NULL, NULL, NULL);
        // For if select failed
        if (select_result < 0) {
            fprintf(stderr, "Error with select: %s\n", strerror(errno));
            if (close(sockfd) < 0) {
                fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                return 1; 
			}
            break;
        }

        // Check STDIN for data
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // Read
            bytes_read = read(STDIN_FILENO, send_buffer, SEND_BUFFER_SIZE);
            // For if read failed
            if (bytes_read < 0) {
                fprintf(stderr, "Error with read: %s\n", strerror(errno));
                if (close(sockfd) < 0) {
                    fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                }
                return 1;
            } else if (bytes_read == 0) {
                // Send empty
                bytes_sent = send(sockfd, send_buffer, bytes_read, 0);
                if (bytes_sent < 0) {
                    fprintf(stderr, "Error with send: %s\n", strerror(errno));
                    if (close(sockfd) < 0) {
                        fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                    }
                    return 1;
                }
                break;
            } else {
                // Send with data 
                bytes_sent = send(sockfd, send_buffer, bytes_read, 0);
                if (bytes_sent < 0) {
                    fprintf(stderr, "Error with send: %s\n", strerror(errno));
                    if (close(sockfd) < 0) {
                        fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                    }
                    return 1;
                }
            }
        }

        // Check sock for any receive data
        if (FD_ISSET(sockfd, &read_fds)) {
            // Receive 
            bytes_received = recv(sockfd, recv_buffer, RECV_BUFFER_SIZE, 0);
            // For if received failed 
            if (bytes_received < 0) {
                fprintf(stderr, "Error with recv: %s\n", strerror(errno));
                if (close(sockfd) < 0) {
                    fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                }
                return 1;
            } else if (bytes_received == 0) {
                // Received an empty packet, terminate
                break;
            } else {
                // Write received data to standard output
                if ((bytes_sent = better_write(STDIN_FILENO, recv_buffer, bytes_received)) < 0) {
                    fprintf(stderr, "Error with better_write: %s\n", strerror(errno));
                    if (close(sockfd) < 0) {
                        fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
                    }
                    return 1;
                }
            }
        }
    }

    // Clean up
    if (close(sockfd) < 0) {
        fprintf(stderr, "Could not close a socket: %s\n", strerror(errno)); 
        return 1; 
    }

    // Success
    return 0;
}
