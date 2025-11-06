# Makefile to build the project
# NOTE: This file must not be changed.

# Parameters
CC = gcc
CFLAGS = -Wall

BIN = bin
CABLE = cable
SRC = src

TX_SERIAL_PORT = /dev/ttyS10
RX_SERIAL_PORT = /dev/ttyS11

# Use CUSTOM_BAUDRATE if provided, otherwise BAUD_RATE
BAUD = $(if $(CUSTOM_BAUDRATE),$(CUSTOM_BAUDRATE),$(BAUD_RATE))
BAUD_RATE = 9600

TX_FILE = penguin.gif
RX_FILE = penguin-received.gif

# Main
.PHONY: all
all: main bin/cable bin/custom_cable

main: $(SRC)/*.c
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^






.PHONY: run_tx
run_tx: main
	if [ -n "$(CUSTOM_MAX_PAYLOAD_SIZE)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_MAX_PAYLOAD_SIZE=$(CUSTOM_MAX_PAYLOAD_SIZE) -o $(BIN)/main $(SRC)/*.c; \
	fi
	./$(BIN)/main $(TX_SERIAL_PORT) $(BAUD) tx $(TX_FILE)






.PHONY: run_rx
run_rx: main
	if [ -n "$(CUSTOM_MAX_PAYLOAD_SIZE)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_MAX_PAYLOAD_SIZE=$(CUSTOM_MAX_PAYLOAD_SIZE) -o $(BIN)/main $(SRC)/*.c; \
	fi
	./$(BIN)/main $(RX_SERIAL_PORT) $(BAUD) rx $(RX_FILE)

.PHONY: check_files
check_files:
	diff -s $(TX_FILE) $(RX_FILE) || exit 0

# Cable
bin/cable: $(CABLE)/cable.c
	$(CC) $(CFLAGS) -o $@ $^

# Custom Cable
.PHONY: bin/custom_cable
bin/custom_cable:
	if [ -n "$(CUSTOM_PROP_DELAY)" ] && [ -n "$(CUSTOM_BAUDRATE)" ] && [ -n "$(CUSTOM_BYTE_ERR)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_PROP_DELAY=$(CUSTOM_PROP_DELAY) -DCUSTOM_BAUDRATE=$(CUSTOM_BAUDRATE) -DCUSTOM_BYTE_ERR=$(CUSTOM_BYTE_ERR) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_PROP_DELAY)" ] && [ -n "$(CUSTOM_BAUDRATE)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_PROP_DELAY=$(CUSTOM_PROP_DELAY) -DCUSTOM_BAUDRATE=$(CUSTOM_BAUDRATE) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_PROP_DELAY)" ] && [ -n "$(CUSTOM_BYTE_ERR)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_PROP_DELAY=$(CUSTOM_PROP_DELAY) -DCUSTOM_BYTE_ERR=$(CUSTOM_BYTE_ERR) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_BAUDRATE)" ] && [ -n "$(CUSTOM_BYTE_ERR)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_BAUDRATE=$(CUSTOM_BAUDRATE) -DCUSTOM_BYTE_ERR=$(CUSTOM_BYTE_ERR) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_PROP_DELAY)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_PROP_DELAY=$(CUSTOM_PROP_DELAY) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_BAUDRATE)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_BAUDRATE=$(CUSTOM_BAUDRATE) -o $@ $(CABLE)/custom_cable.c; \
	elif [ -n "$(CUSTOM_BYTE_ERR)" ]; then \
		$(CC) $(CFLAGS) -DCUSTOM_BYTE_ERR=$(CUSTOM_BYTE_ERR) -o $@ $(CABLE)/custom_cable.c; \
	else \
		$(CC) $(CFLAGS) -o $@ $(CABLE)/custom_cable.c; \
	fi

.PHONY: run_cable
run_cable: bin/cable
	@which -s socat || { echo "Error: Could not find socat. Install socat and try again."; exit 1; }
	sudo ./$(BIN)/cable

.PHONY: run_custom_cable
run_custom_cable: bin/custom_cable
	@which -s socat || { echo "Error: Could not find socat. Install socat and try again."; exit 1; }
	sudo ./$(BIN)/custom_cable

# Clean
.PHONY: clean
clean:
	rm -f $(BIN)/main
	rm -f $(BIN)/cable
	rm -f $(RX_FILE)
