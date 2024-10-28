CC = cc
CFLAGS = -O3 -Wall

TARGETS = send_udp receive_udp reply_udp

all: $(TARGETS)

%: %.o
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGETS)
