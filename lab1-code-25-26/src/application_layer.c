// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
  LinkLayer ll;
  strncpy(ll.serialPort, serialPort, sizeof(ll.serialPort) - 1);
  ll.serialPort[sizeof(ll.serialPort) - 1] = '\0';
  ll.baudRate = baudRate;
  ll.nRetransmissions = nTries;
  ll.timeout = timeout;

  if (strcmp(role, "tx") == 0)
    ll.role = LlTx;
  else if (strcmp(role, "rx") == 0)
    ll.role = LlRx;
  else {
    fprintf(stderr, "Failed to set link layer role\n");
    return;
  }

  if (llopen(ll) < 0) {
    fprintf(stderr, "Failed to open link layer\n");
    return;
  }

  if (ll.role == LlTx) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
      fprintf(stderr, "Failed to open file %s\n", filename);
      llclose();
      return;
    }

    // Buffer to store chunks of 1000 bytes
    unsigned char buffer[MAX_PAYLOAD_SIZE]; // MAX_PAYLOAD_SIZE = 1000
    int data_bytes_read;

    printf("Starting file transmission: %s\n", filename);

    // Read file in chunks of 1000 bytes
    while ((data_bytes_read = fread(buffer, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
      printf("Read %d bytes from file\n", data_bytes_read);

      // Send chunk via link layer
      int data_bytes_written = llwrite(buffer, data_bytes_read);

      if (data_bytes_written != data_bytes_read) {
        // TODO: redo the error print
        fprintf(stderr, "Maximum retransmissions reached\n");
        fclose(file);
        llclose();
        return;
      }
    }

    fclose(file);
    printf("File transmission completed\n");
  } else if (ll.role == LlRx) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
      fprintf(stderr, "Failed to create file %s\n", filename);
      llclose();
      return;
    }

    printf("Starting file reception: %s\n", filename);

    unsigned char buffer[MAX_PAYLOAD_SIZE];
    int bytes_read;
    int total_bytes_received = 0;

    // Receive packets until END packet (for M5, you'll parse packet type)
    // For M3/M4, just receive until connection closed (simplified)
    while ((bytes_read = llread(buffer)) > 0) {
      printf("Received %d bytes\n", bytes_read);
      int writen = fwrite(buffer, 1, bytes_read, file);
      total_bytes_received += writen;
      printf("Written %d bytes to file, total received: %d bytes\n", writen,
             total_bytes_received);
      // i think we can remove this fflush when m5 is done since fclose will
      // flush
      fflush(file); // Force write to disk immediately
                    // TODO (M5): Check for END packet to break
    }

    printf("Total bytes received: %d\n", total_bytes_received);

    fclose(file);
    printf("File reception completed\n");
  }

  if (llclose() < 0) {
    fprintf(stderr, "Failed to close link layer\n");
    return;
  }
}
