// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
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

        // Buffer para armazenar chunks de 1000 bytes
        unsigned char buffer[MAX_PAYLOAD_SIZE]; // MAX_PAYLOAD_SIZE = 1000
        int bytesRead;

        printf("Starting file transmission: %s\n", filename);

        // Ler arquivo em chunks de 1000 bytes
        while ((bytesRead = fread(buffer, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
            printf("Read %d bytes from file\n", bytesRead);
            
            // Enviar chunk via link layer
            if (llwrite(buffer, bytesRead) < 0) {
                fprintf(stderr, "Failed to send data chunk\n");
                fclose(file);
                llclose();
                return;
            }
            
            printf("Sent %d bytes\n", bytesRead);
        }

        fclose(file);
        printf("File transmission completed\n");

    } else if (role == LlRx) {

    }

    if (llclose(ll) < 0) {
        fprintf(stderr, "Failed to close link layer\n");
        return;
    }
}
