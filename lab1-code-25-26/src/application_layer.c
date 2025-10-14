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
  }

  if (llclose() < 0) {
    fprintf(stderr, "Failed to close link layer\n");
    return;
  }
}
