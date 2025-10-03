// Alarm example using sigaction.
// This example shows how to configure an alarm using the sigaction function.
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
//              Rui Prior [rcprior@fc.up.pt]

// TODO: should i clear buf memory?
// TODO: check VMIN and VTIME to maybe read more than 1 byte at a time

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "serialport.h"
#include "supervision_state.h"

#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400
#define BUF_SIZE 256

enum SUPERVISION_STATE set_frame_state = START;
enum SUPERVISION_STATE ua_frame_state = START;

const char *tx_serial_port;
const char *rx_serial_port;
int tx_fd = -1;
int rx_fd = -1;
struct termios tx_oldtio;
struct termios rx_oldtio;
unsigned char set_frame[5] = {FLAG, A_SENDER, C_SET, A_SENDER ^ C_SET, FLAG};
unsigned char ua_frame[5] = {FLAG, A_RECEIVER, C_UA, A_RECEIVER ^ C_UA, FLAG};

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

int send_set_frame() {
  set_frame_state = START;
  int bytes = writeBytesSerialPort(set_frame, 5, tx_fd);
  printf("%d set_bytes sent\n", bytes);
  return bytes;
}

int send_ua_frame() {
  ua_frame_state = START;
  int bytes = writeBytesSerialPort(ua_frame, 5, rx_fd);
  printf("%d ua_bytes sent\n", bytes);
  return bytes;
}

int main(int argc, char *argv[]) {

  if (argc < 3) {
    printf("Incorrect program usage\n"
           "Usage: %s <tx_serial_port> <rx_serial_port>\n"
           "Example: %s /dev/ttyS0 /dev/ttyUSB0\n",
           argv[0], argv[0]);
    exit(-1);
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

  unsigned char buf[BUF_SIZE] = {0};
  int cur_num_bytes = 0;
  unsigned char byte;

  // Set alarm function handler.
  // Install the function signal to be automatically invoked when the timer
  // expires, invoking in its turn the user function alarmHandler
  struct sigaction act = {0};
  act.sa_handler = &alarmHandler;
  if (sigaction(SIGALRM, &act, NULL) == -1) {
    perror("sigaction");
    exit(-1);
  }

  printf("Alarm configured\n");

  while (alarmCount <= max_alarm_count) {
    if (alarmEnabled == FALSE) {
      memset(buf, 0, BUF_SIZE);
      cur_num_bytes = 0;
      if (send_set_frame() < 0) {
        perror("send_set_frame");
        exit(-1);
      }
      alarm(timeout); // Set alarm to be triggered in 3s
      alarmEnabled = TRUE;
    }

    // TODO: check VMIN and VTIME to maybe read more than 1 byte at a time
    byte = '\0';
    int bytes = readByteSerialPort(&byte, rx_fd);

    if (bytes < 0) {
      if (errno == EINTR) {
        // read was interrupted by the alarm signal, just continue
        if (alarmEnabled == FALSE) {
          printf("UA verification interrupted by alarm\n");
        }
        continue;
      }
      perror("readByteSerialPort");
      exit(-1);
    } else if (bytes == 0) {
      continue; // No byte received, try again
    }

    printf("set_byte = 0x%02X\n", byte);

    buf[cur_num_bytes] = byte;
    cur_num_bytes += bytes;

    if (set_frame_state == START) {
      if (byte == FLAG) {
        set_frame_state = FLAG_RCV;
      }
    } else if (set_frame_state == FLAG_RCV) {
      if (byte == A_SENDER) {
        set_frame_state = A_RCV;
      } else if (byte == FLAG) {
        set_frame_state = FLAG_RCV;
        continue;
      } else {
        set_frame_state = START;
      }
    } else if (set_frame_state == A_RCV) {
      if (byte == C_SET) {
        set_frame_state = C_RCV;
      } else if (byte == FLAG) {
        set_frame_state = FLAG_RCV;
      } else {
        set_frame_state = START;
      }
    } else if (set_frame_state == C_RCV) {
      if (byte == (buf[1] ^ buf[2])) {
        set_frame_state = BCC_OK;
      } else if (byte == FLAG) {
        set_frame_state = FLAG_RCV;
      } else {
        set_frame_state = START;
      }
    } else if (set_frame_state == BCC_OK) {
      if (byte == FLAG) {
        set_frame_state = SUCCESS;

      } else {
        set_frame_state = START;
      }
    }
    if (set_frame_state == SUCCESS) {
      bytes = send_ua_frame();
      memset(buf, 0, BUF_SIZE);
      cur_num_bytes = 0;

      while (alarmEnabled) {

        // TODO: check VMIN and VTIME to maybe read more than 1 byte at a time
        byte = '\0';
        int bytes = readByteSerialPort(&byte, tx_fd);

        if (bytes < 0) {
          if (errno == EINTR) {
            // read was interrupted by the alarm signal, just continue
            if (alarmEnabled == FALSE) {
              printf("UA verification interrupted by alarm\n");
              break;
            }
            continue;
          }
          perror("readByteSerialPort");
          exit(-1);
        } else if (bytes == 0) {
          continue; // No byte received, try again
        }

        printf("ua_byte = 0x%02X\n", byte);

        buf[cur_num_bytes] = byte;
        cur_num_bytes += bytes;

        if (ua_frame_state == START) {
          if (byte == FLAG) {
            ua_frame_state = FLAG_RCV;
          }
        } else if (ua_frame_state == FLAG_RCV) {
          if (byte == A_RECEIVER) {
            ua_frame_state = A_RCV;
          } else if (byte == FLAG) {
            ua_frame_state = FLAG_RCV;
          } else {
            ua_frame_state = START;
          }
        } else if (ua_frame_state == A_RCV) {
          if (byte == C_UA) {
            ua_frame_state = C_RCV;
          } else if (byte == FLAG) {
            ua_frame_state = FLAG_RCV;
          } else {
            ua_frame_state = START;
          }
        } else if (ua_frame_state == C_RCV) {
          if (byte == (buf[1] ^ buf[2])) {
            ua_frame_state = BCC_OK;
          } else if (byte == FLAG) {
            ua_frame_state = FLAG_RCV;
          } else {
            ua_frame_state = START;
          }
        } else if (ua_frame_state == BCC_OK) {
          if (byte == FLAG) {
            ua_frame_state = SUCCESS;

          } else {
            ua_frame_state = START;
          }
        }
        if (ua_frame_state == SUCCESS) {
          printf("SUCCESS\n");
          break;
        }
      }

      if (ua_frame_state == SUCCESS) {
        // Disable pending alarms, if any
        alarm(0);
        break;
      }
    }
  }

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
