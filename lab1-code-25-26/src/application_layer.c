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

    /*if (ll.role == LlTx) {
        //ler o gif

    } else if (role == LlRx) {

    }*/

    if (llclose(ll) < 0) {
        fprintf(stderr, "Failed to close link layer\n");
        return;
    }
}
