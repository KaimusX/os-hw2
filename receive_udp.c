#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 65536  // 2^16 bytes for UDP buffer

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
    const char *port_name;
    uint16_t port;
    int sockfd;
    struct sockaddr_in addr; 
    char buffer[BUFFER_SIZE];

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments: %s <port name>\n", ((argc > 0) ? argv[0] : "./receive_udp"));
		return 1;
    }

    port_name = argv[1];

    if (convert_port_name(&port, port_name) < 0) {
        fprintf(stderr, "Error with convert_port_name\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "Could not open a UDP socket: %s\n", strerror(errno));
        return 1; 
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Could not bind a UDP socket: %s\n", strerror(errno));
        if (close(sockfd) < 0) {
            fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));      
        }
        return 1;
    }

    for (;;) {
        ssize_t recv_len = recv(sockfd, buffer, sizeof(buffer), 0);
        if (recv_len < 0) {
            fprintf(stderr, "Error with recv: %s\n", strerror(errno));
            if (close(sockfd) < 0) {
                fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));      
            }
            return 1;
        }

        // If an empty packet is received, terminate the program
        if (recv_len == 0) break;

        if (better_write(1, buffer, recv_len) < 0) {
            fprintf(stderr, "Error with better_write\n");
            if (close(sockfd) < 0) {
                fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));      
            }
            return 1;
        }
    }

    if (close(sockfd) < 0) {
        fprintf(stderr, "Could not close a UDP socket: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
