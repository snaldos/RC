// Alarm example using sigaction.
// This example shows how to configure an alarm using the sigaction function.
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
//              Rui Prior [rcprior@fc.up.pt]

#include "serialport.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400

const char *tx_serial_port;
const char *rx_serial_port;
int tx_fd = -1;
int rx_fd = -1;
struct termios tx_oldtio;
struct termios rx_oldtio;

int alarmEnabled = FALSE;
int alarmCount = 0;
int max_alarm_count = 3;
int timeout = 3;

// Alarm function handler.
// This function will run whenever the signal SIGALRM is received.
void alarmHandler(int signal) {
  alarmEnabled = FALSE;
  alarmCount++;

  printf("Alarm #%d received\n", alarmCount);
}

// int

int main(int argc, char *argv[]) {

  if (argc < 3) {
    printf("Incorrect program usage\n"
           "Usage: %s <tx_serial_port> <rx_serial_port>\n"
           "Example: %s /dev/ttyS0 /dev/ttyUSB0\n",
           argv[0], argv[0]);
    exit(1);
  }

  tx_serial_port = argv[1];
  rx_serial_port = argv[2];

  if (openSerialPort(tx_serial_port, BAUDRATE, &tx_fd, &rx_oldtio) < 0) {
    perror("tx : openSerialPort");
    exit(-1);
  }

  printf("tx_serial_port %s opened\n", tx_serial_port);

  if (openSerialPort(rx_serial_port, BAUDRATE, &rx_fd, &rx_oldtio) < 0) {
    perror("rx : openSerialPort");
    exit(-1);
  }

  printf("rx_serial_port %s opened\n", rx_serial_port);

  // Set alarm function handler.
  // Install the function signal to be automatically invoked when the timer
  // expires, invoking in its turn the user function alarmHandler
  struct sigaction act = {0};
  act.sa_handler = &alarmHandler;
  if (sigaction(SIGALRM, &act, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("Alarm configured\n");

  while (alarmCount <= max_alarm_count) {
    if (alarmEnabled == FALSE) {
      alarm(timeout); // Set alarm to be triggered in 3s
      alarmEnabled = TRUE;
    }
  }

  printf("Ending program\n");

  // Close serial port
  if (closeSerialPort(tx_fd, &tx_oldtio) < 0) {
    perror("tx : closeSerialPort");
    exit(-1);
  }

  printf("tx_serial_port %s closed\n", tx_serial_port);

  // Close serial port
  if (closeSerialPort(rx_fd, &rx_oldtio) < 0) {
    perror("rx : closeSerialPort");
    exit(-1);
  }

  printf("rx_serial_port %s closed\n", rx_serial_port);

  return 0;
}
