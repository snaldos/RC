// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "utils.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

/*
    TODO: note about sleep():
    Slide 15: "I, SET and DISC frames are protected by a timer"
    Your sleep(1) is a temporary hack — for M4, you must use alarm() + signal
   handler with ll_config.timeout

*/

static LinkLayer ll_config;
static int ll_opened = 0;

// TODO: decide when N(s) = 0 or N(s) = 1

static unsigned char next_iframe_nr = 0; // N(s) for next I-frame to send
static unsigned char expected_iframe_nr =
    0; // N(r) for expected I-frame to receive

static volatile int alarm_triggered = FALSE;
static volatile int alarm_count = 0;

const unsigned int MAX_IFRAME_SIZE = (2 * (4 + MAX_PAYLOAD_SIZE) + 2);
// Pseudocode for destuffing
// for (int i = 0; i < stuffed_len; ) {
//     if (stuffed[i] == 0x7D) {
//         destuffed[dst_len++] = stuffed[i+1] ^ 0x20;
//         i += 2;
//     } else {
//         destuffed[dst_len++] = stuffed[i];
//         i += 1;
//     }
// }

// Signal handler for alarm (timeout)
static void alarm_handler(int signal) {
  alarm_triggered = TRUE;
  alarm_count++;
}

// Function to setup alarm signal handler
static int setup_alarm_handler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = alarm_handler;

  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("sigaction");
    return -1;
  }
  return 0;
}

static int apply_byte_stuffing(const unsigned char *src, int len,
                               unsigned char *dest) {

  if (src == NULL || len < 0 || dest == NULL) {
    return -1;
  }
  int dest_len = 0;
  for (int i = 0; i < len; i++) {
    const unsigned char cur_byte = src[i];
    if (cur_byte == FLAG || cur_byte == ESCAPE_OCTET) {
      dest[dest_len++] = ESCAPE_OCTET;
      dest[dest_len++] = cur_byte ^ XOR_OCTET;
    } else {
      dest[dest_len++] = cur_byte;
    }
  }
  return dest_len;
}

static int apply_byte_destuffing(const unsigned char *src, int len,
                                 unsigned char *dest) {
  if (src == NULL || len < 0 || dest == NULL) {
    return -1;
  }

  int dest_len = 0;
  for (int i = 0; i < len;) {
    if (src[i] == ESCAPE_OCTET && i + 1 < len) {
      dest[dest_len++] = src[i + 1] ^ XOR_OCTET;
      i += 2;
    } else {
      dest[dest_len++] = src[i];
      i++;
    }
  }
  return dest_len;
}

static int create_sframe(unsigned char *frame) {
  if (frame == NULL) {
    return -1;
  }

  // Supervision Frame format: F | A | C | BCC | F

  int idx = 0;
  frame[idx++] = FLAG;
  if (ll_config.role == LlTx) {
    frame[idx++] = A_SENDER;
    frame[idx++] = C_SET;
    frame[idx++] = A_SENDER ^ C_SET;
  } else if (ll_config.role == LlRx) {
    frame[idx++] = A_RECEIVER;
    frame[idx++] = C_UA;
    frame[idx++] = A_RECEIVER ^ C_UA;
  }
  frame[idx++] = FLAG;

  return idx;
}

static int create_iframe(const unsigned char *data, int data_size,
                         unsigned char *frame) {

  if (data == NULL || frame == NULL || data_size <= 0 ||
      data_size > MAX_PAYLOAD_SIZE) {
    return -1;
  }

  // Information Frame format: F | A | C | BCC1 | D1 ... DN | BCC2 | F

  int idx = 0;
  frame[idx++] = FLAG;
  frame[idx++] = A_SENDER;

  unsigned char C = (next_iframe_nr == 0) ? C_I0 : C_I1;
  frame[idx++] = C;

  frame[idx++] = A_SENDER ^ C;

  // Copy data into frame (payload)
  for (int i = 0; i < data_size; i++) {
    frame[idx++] = data[i];
  }

  unsigned char bcc2 = 0;
  for (int i = 0; i < data_size; i++) {
    bcc2 ^= data[i];
  }
  frame[idx++] = bcc2;

  frame[idx++] = FLAG;

  // total unstuffed frame length
  return idx;
}

static int send_sframe(unsigned char control) {
  unsigned char frame[SFRAME_SIZE];
  frame[0] = FLAG;
  frame[1] = A_RECEIVER;
  frame[2] = control;
  frame[3] = frame[1] ^ frame[2];
  frame[4] = FLAG;

  return writeBytesSerialPort(frame, SFRAME_SIZE);
}

static int send_iframe(const unsigned char *buf, int buf_size) {

  unsigned char raw_frame[MAX_IFRAME_SIZE]; // you'll define this macro next
  int raw_len = create_iframe(buf, buf_size, raw_frame);
  if (raw_len < 0) {
    return -1;
  }

  // byte stuffing
  int body_len = raw_len - 2;
  unsigned char *body = &raw_frame[1];

  // note: in theory int max_stuffed_body_len = max_iframe_size - 2;
  unsigned char stuffed_body[MAX_IFRAME_SIZE];
  int stuffed_body_len = apply_byte_stuffing(body, body_len, stuffed_body);
  if (stuffed_body_len < 0) {
    return -1;
  }

  // note: in theory int final_iframe_len = stuffed_body_len + 2;

  unsigned char final_iframe[MAX_IFRAME_SIZE];
  final_iframe[0] = FLAG;
  memcpy(&final_iframe[1], stuffed_body, stuffed_body_len);
  final_iframe[1 + stuffed_body_len] = FLAG;
  int final_iframe_len = 1 + stuffed_body_len + 1;

  int bytes_written = writeBytesSerialPort(final_iframe, final_iframe_len);
  printf("LL: Sent %d bytes\n", bytes_written);
  return bytes_written;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {

  if (openSerialPort(connectionParameters.serialPort,
                     connectionParameters.baudRate) < 0) {
    perror("openSerialPort");
    return -1;
  }

  printf("Serial port %s opened\n", connectionParameters.serialPort);
  ll_config = connectionParameters;
  ll_opened = 1;

  if (setup_alarm_handler() < 0) {
    return -1;
  }

  printf("Alarm handler set up\n");

  enum SUPERVISION_STATE sframe_state = START;

  if (ll_config.role == LlTx) {
    unsigned char set_frame[SFRAME_SIZE] = {0};

    if (create_sframe(set_frame) != SFRAME_SIZE) {
      return -1;
    }

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.

    int bytes_written = writeBytesSerialPort(set_frame, SFRAME_SIZE);
    if (bytes_written < 0) {
      perror("writeBytesSerialPort");
      return -1;
    }
    printf("%d bytes written to serial port\n", bytes_written);

    // TODO: validate the use of sleep 1
    // Wait until all bytes have been written to the serial port
    sleep(1);

    int idx = 0;
    unsigned char ua_frame[SFRAME_SIZE] = {0};

    while (sframe_state != SUCCESS) {
      // NOTE: This while() cycle is a simple example showing how to read from
      // the serial port. It must be changed in order to respect the
      // specifications of the protocol indicated in the Lab guide.

      // Read one byte from serial port
      // NOTE: This function may return even if no byte was received, which may
      // not be true.
      unsigned char byte;
      int bytes_read = readByteSerialPort(&byte);

      if (bytes_read < 0) {
        perror("readByteSerialPort");
        return -1;
      } else if (bytes_read == 0) {
        continue; // No byte received, try again
      }

      if (sframe_state == START) {
        idx = 0;
        if (byte == FLAG) {
          printf("Tracking UA FRAME...\n");
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == FLAG_RCV) {
        if (byte == A_RECEIVER) {
          sframe_state = A_RCV;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == A_RCV) {
        if (byte == C_UA) {
          sframe_state = C_RCV;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == C_RCV) {
        if (byte == (ua_frame[1] ^ ua_frame[2])) {
          sframe_state = BCC_OK;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == BCC_OK) {
        if (byte == FLAG) {
          sframe_state = SUCCESS;
        } else {
          sframe_state = START;
        }
      }

      if (sframe_state != START) {
        ua_frame[idx++] = byte;
      }

      if (sframe_state == SUCCESS) {
        printf("UA frame received successfully\n");
      }
    }
  } else if (ll_config.role == LlRx) {

    int idx = 0;
    unsigned char set_frame[SFRAME_SIZE] = {0};

    while (sframe_state != SUCCESS) {
      // Read one byte from serial port.
      // NOTE: You must check how many bytes were actually read by reading the
      // return value. In this example, we assume that the byte is always read,
      // which may not be true.
      unsigned char byte;
      int bytes_read = readByteSerialPort(&byte);

      if (bytes_read < 0) {
        perror("readByteSerialPort");
        return -1;
      } else if (bytes_read == 0) {
        continue; // No byte received, try again
      }

      if (sframe_state == START) {
        if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        }
      } else if (sframe_state == FLAG_RCV) {
        if (byte == A_SENDER) {
          sframe_state = A_RCV;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == A_RCV) {
        if (byte == C_SET) {
          sframe_state = C_RCV;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == C_RCV) {
        if (byte == (set_frame[1] ^ set_frame[2])) {
          sframe_state = BCC_OK;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
          idx = 0;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == BCC_OK) {
        if (byte == FLAG) {
          sframe_state = SUCCESS;

        } else {
          sframe_state = START;
        }
      }

      if (sframe_state != START) {
        set_frame[idx++] = byte;
      }

      if (sframe_state == SUCCESS) {
        printf("SET frame received successfully\n");
        unsigned char ua_frame[SFRAME_SIZE] = {0};

        if (create_sframe(ua_frame) != SFRAME_SIZE) {
          return -1;
        }

        int bytes_written = writeBytesSerialPort(ua_frame, SFRAME_SIZE);

        if (bytes_written < 0) {
          return -1;
        }
        printf("UA frame sent\n");

        // TODO: validate the use of sleep
        // Wait until all bytes have been written to the serial port
        sleep(1);
      }
    }

    printf("Total bytes received: %d\n", idx);
  }

  return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
  if (!ll_opened || ll_config.role != LlTx) {
    return -1;
  }

  if (buf == NULL || bufSize <= 0 || bufSize > MAX_PAYLOAD_SIZE) {
    return -1;
  }
  // TODO: Implement this function

  int max_attempts = ll_config.nRetransmissions;
  for (int attempts = 0; attempts < max_attempts; attempts++) {
    alarm_triggered = FALSE;

    // Start timeout timer
    alarm(ll_config.timeout);

    enum SUPERVISION_STATE ack_frame_state = START;
    unsigned char ack_frame[SFRAME_SIZE] = {0};
    int idx = 0;

    int bytes_sent = send_iframe(buf, bufSize);
    if (bytes_sent < 0) {
      return -1;
    }

    while (!alarm_triggered || ack_frame_state != SUCCESS) {
      unsigned char byte;
      int bytes_read = readByteSerialPort(&byte);
      if (bytes_read < 0) {
        if (errno == EINTR) {
          // read was interrupted by the alarm signal, just continue
          if (alarm_triggered)
            break;

          continue;
        }

        alarm(0);
        perror("readByteSerialPort");
        return -1;
      }
      // No byte received, try again
      else if (bytes_read == 0)
        continue;

      // (REPEATED CODE REFACTOR LATER)
      if (ack_frame_state == START) {
        idx = 0;
        if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
        }
      } else if (ack_frame_state == FLAG_RCV) {
        if (byte == A_RECEIVER) {
          ack_frame_state = A_RCV;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
          idx = 0;
        } else {
          ack_frame_state = START;
        }
      } else if (ack_frame_state == A_RCV) {
        // Check for RR or REJ
        if (byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 ||
            byte == C_REJ1) {
          ack_frame_state = C_RCV;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
          idx = 0;
        } else {
          ack_frame_state = START;
        }
      } else if (ack_frame_state == C_RCV) {
        if (byte == (ack_frame[1] ^ ack_frame[2])) {
          ack_frame_state = BCC_OK;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
          idx = 0;
        } else {
          ack_frame_state = START;
        }
      } else if (ack_frame_state == BCC_OK) {
        if (byte == FLAG) {
          ack_frame_state = SUCCESS;
        } else {
          ack_frame_state = START;
        }
      }

      if (ack_frame_state != START) {
        ack_frame[idx++] = byte;
      }
    }

    // Stop timeout timer
    alarm(0);
    if (ack_frame_state == SUCCESS) {
      unsigned char ack_frame_c = ack_frame[2];

      // Extract N(r) from control byte
      unsigned char nr = (ack_frame_c >> 7) & 0x01;

      if ((ack_frame_c == C_RR0) || (ack_frame_c == C_RR1)) {

        unsigned char expected_next_iframe_nr = (next_iframe_nr + 1) % 2;
        if (nr == expected_next_iframe_nr) {
          // Should always happen if RR received
          printf("LL: Received RR%d - Frame acknowledged\n", nr);
          next_iframe_nr = (next_iframe_nr + 1) % 2;
          return bufSize;
        } else
          // In theory never happens but who knows...
          // Why? Because next_iframe_nr gets updated solely on RR received
          // which prooves correctness assuming RR sends are correct
          printf("LL: Received RR but with wrong N(r), expected %d\n",
                 expected_next_iframe_nr);
      } else if (ack_frame_c == C_REJ0 || ack_frame_c == C_REJ1) {
        printf("LL: Received REJ%d - Retransmitting immediately\n", nr);
        continue;
      }
    } else if (alarm_triggered) {
      printf("LL: Timeout occurred, retransmitting (attempt %d of %d)\n",
             attempts + 1, max_attempts);
    }

    // NOTE: PSEUDACODE
    // while (attempts < max_attempts) {
    //   send_frame(); // full stuffed I-frame

    //   start_timer(saved_ll.timeout);

    //   wait_for_ack_or_rej(); // block until RR/REJ or timeout

    //   if (got_valid_RR_with_expected_Nr()) {
    //     toggle_sequence_number();
    //     return bufSize; // success
    //   }
    //   // else: retransmit (same seq num)
    //   attempts++;
    // }

    // return -1; // all retries failed
  }

  printf("LL: Maximum retransmissions reached, giving up\n");
  return -1;
}
////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  if (!ll_opened || ll_config.role != LlRx || packet == NULL) {
    return -1;
  }

  enum SUPERVISION_STATE iframe_state = START;
  unsigned char raw_frame[MAX_IFRAME_SIZE]; // stuffed body
  int idx = 0;

  while (iframe_state != SUCCESS) {
    unsigned char byte;
    int bytes_read = readByteSerialPort(&byte);

    if (bytes_read < 0) {
      perror("readByteSerialPort");
      return -1;
    } else if (bytes_read == 0) {
      continue; // No byte received, try again
    }

    if (iframe_state == START) {
      idx = 0;
      if (byte == FLAG) {
        printf("start of stuffed FRAME...\n");
        iframe_state = FLAG_RCV;
      }
    } else if (iframe_state == FLAG_RCV) {
      if (byte == FLAG) {
        printf("end of stuffed FRAME...\n");
        iframe_state = SUCCESS;
      }
    }

    if (iframe_state != START) {
      raw_frame[idx++] = byte;
    }
  }

  return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose() {
  if (closeSerialPort() < 0) {
    perror("closeSerialPort");
    return -1;
  }

  printf("Serial port closed\n");

  return 0;
}
