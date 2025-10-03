// Example of how to write to the serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "serialport.h"
#include "supervision_state.h"

#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400
#define BUF_SIZE 256

enum SUPERVISION_STATE set_frame_state = START;

int fd = -1;           // File descriptor for open serial port
struct termios oldtio; // Serial port settings to restore on closing
volatile int STOP = FALSE;

// ---------------------------------------------------
// MAIN
// ---------------------------------------------------
int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Incorrect program usage\n"
           "Usage: %s <SerialPort> <Baudrate>\n"
           "Example: %s /dev/ttyS0 9600\n",
           argv[0], argv[0]);
    exit(-1);
  }

  // Open serial port device for reading and writing, and not as controlling tty
  // because we don't want to get killed if linenoise sends CTRL-C.
  //
  // NOTE: See the implementation of the serial port library in "serial_port/".
  const char *serialPort = argv[1];

  if (openSerialPort(serialPort, BAUDRATE, &fd, &oldtio) < 0) {
    perror("openSerialPort");
    exit(-1);
  }

  printf("Serial port %s opened\n", serialPort);

  // Create string to send
  unsigned char buf[BUF_SIZE] = {0};

  buf[0] = FLAG;
  buf[1] = A_SENDER;
  buf[2] = C_SET;
  buf[3] = A_SENDER ^ C_SET;
  buf[4] = FLAG;

  // In non-canonical mode, '\n' does not end the writing.
  // Test this condition by placing a '\n' in the middle of the buffer.
  // The whole buffer must be sent even with the '\n'.

  int bytes = writeBytesSerialPort(buf, BUF_SIZE, fd);
  printf("%d bytes written to serial port\n", bytes);

  // Wait until all bytes have been written to the serial port
  sleep(1);

  int nBytesBuf = 0;

  while (STOP == FALSE) {
    // NOTE: This while() cycle is a simple example showing how to read from the
    // serial port. It must be changed in order to respect the specifications of
    // the protocol indicated in the Lab guide.

    // Read one byte from serial port
    // NOTE: This function may return even if no byte was received, which may
    // not be true.
    unsigned char byte;
    int bytes = readByteSerialPort(&byte, fd);

    if (bytes < 0) {
      perror("readByteSerialPort");
      exit(-1);
    } else if (bytes == 0) {
      continue; // No byte received, try again
    }

    buf[nBytesBuf] = byte;
    nBytesBuf += bytes;

    if (set_frame_state == START) {
      if (byte == FLAG) {
        printf("Tracking UA FRAME...\n");
        set_frame_state = FLAG_RCV;
      } else {
        set_frame_state = START;
        STOP = TRUE;
      }
    } else if (set_frame_state == FLAG_RCV) {
      if (byte == A_RECEIVER) {
        set_frame_state = A_RCV;
      } else {
        set_frame_state = START;
        STOP = TRUE;
      }
    } else if (set_frame_state == A_RCV) {
      if (byte == C_UA) {
        set_frame_state = C_RCV;
      } else {
        set_frame_state = START;
        STOP = TRUE;
      }
    } else if (set_frame_state == C_RCV) {
      if (byte == (buf[1] ^ buf[2])) {
        set_frame_state = BCC_OK;
      } else {
        set_frame_state = START;
        STOP = TRUE;
      }
    } else if (set_frame_state == BCC_OK) {
      if (byte == FLAG) {
        set_frame_state = SUCCESS;

      } else {
        set_frame_state = START;
        STOP = TRUE;
      }
    }
    if (set_frame_state == SUCCESS) {
      printf("UA frame received successfully\n");
      STOP = TRUE;
    }
  }

  // Close serial port
  if (closeSerialPort(fd, &oldtio) < 0) {
    perror("closeSerialPort");
    exit(-1);
  }

  printf("Serial port %s closed\n", serialPort);

  return 0;
}
