# Parameters
CC = gcc
CFLAGS = -Wall

TX_SERIAL_PORT = /dev/ttyS10
RX_SERIAL_PORT = /dev/ttyS11
BAUD_RATE = 9600

TX_FILE = penguin.gif
RX_FILE = penguin-received.gif

# Main targets
.PHONY: all
all: alarm cable read write

alarm: alarm_sigaction.c supervision_state.h
	$(CC) $(CFLAGS) -o $@ alarm_sigaction.c

cable: cable.c supervision_state.h
	$(CC) $(CFLAGS) -o $@ cable.c

read: read_noncanonical.c supervision_state.h
	$(CC) $(CFLAGS) -o $@ read_noncanonical.c

write: write_noncanonical.c supervision_state.h
	$(CC) $(CFLAGS) -o $@ write_noncanonical.c

# Example run targets (customize as needed)
.PHONY: run_read
run_read: read
	./read $(RX_SERIAL_PORT) $(BAUD_RATE)

.PHONY: run_write
run_write: write
	./write $(TX_SERIAL_PORT) $(BAUD_RATE)

.PHONY: run_cable
run_cable: cable
	@which socat || { echo "Error: Could not find socat. Install socat and try again."; exit 1; }
	sudo ./cable

# Clean
.PHONY: clean
clean:
	rm -f alarm cable read write $(RX_FILE)
