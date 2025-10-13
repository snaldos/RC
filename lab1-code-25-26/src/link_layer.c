// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

static LinkLayer ll_config;
static int ll_opened = 0;

// TODO: decide when N(s) = 0 or N(s) = 1
// Global sequence number for I-frames
static unsigned char next_seq_num = 0;

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

static int create_iframe(const unsigned char *data, int dataSize,
                         unsigned char *frame) {

  if (data == NULL || frame == NULL || dataSize <= 0 ||
      dataSize > MAX_PAYLOAD_SIZE) {
    return -1;
  }

  // Information Frame format: F | A | C | BCC1 | D1 ... DN | BCC2 | F

  int idx = 0;
  frame[idx++] = FLAG;
  frame[idx++] = A_SENDER;

  unsigned char C = (next_seq_num == 0) ? C_0 : C_1;
  frame[idx++] = C;

  frame[idx++] = A_SENDER ^ C;

  // Copy data into frame (payload)
  for (int i = 0; i < dataSize; i++) {
    frame[idx++] = data[i];
  }

  unsigned char bcc2 = 0;
  for (int i = 0; i < dataSize; i++) {
    bcc2 ^= data[i];
  }
  frame[idx++] = bcc2;

  frame[idx++] = FLAG;

  // total unstuffed frame length
  return idx;
}

static int transmit_frame(const unsigned char *buf, int bufSize) {

  int max_iframe_size = (2 * (4 + MAX_PAYLOAD_SIZE) + 2);
  unsigned char raw_frame[max_iframe_size]; // you'll define this macro next
  int raw_len = create_iframe(buf, bufSize, raw_frame);
  if (raw_len < 0) {
    return -1;
  }

  int bytesWritten = writeBytesSerialPort(raw_frame, raw_len);
  printf("LL: Sent %d bytes\n", bytesWritten);
  return bytesWritten;

  // pseudo code
  // int body_len = raw_len - 2;
  // unsigned char *body = &raw_frame[1];

  // unsigned char stuffed_body[MAX_IFRAME_SIZE];
  // int stuffed_len = apply_stuffing(body, body_len, stuffed_body);

  // unsigned char final_frame[MAX_IFRAME_SIZE];
  // final_frame[0] = FLAG;
  // memcpy(&final_frame[1], stuffed_body, stuffed_len);
  // final_frame[1 + stuffed_len] = FLAG;
  // int final_len = 1 + stuffed_len + 1;

  // int written = writeBytesSerialPort(final_frame, final_len);
  // return written;
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

  volatile int STOP = FALSE;
  enum SUPERVISION_STATE set_frame_state = START;

  if (ll_config.role == LlTx) {
    unsigned char set_frame[SFRAME_SIZE] = {0};

    if (create_sframe(set_frame) != SFRAME_SIZE) {
      return -1;
    }

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.

    int bytes = writeBytesSerialPort(set_frame, SFRAME_SIZE);
    printf("%d bytes written to serial port\n", bytes);

    // Wait until all bytes have been written to the serial port
    sleep(1);

    int nBytesBuf = 0;

    while (STOP == FALSE) {
      // NOTE: This while() cycle is a simple example showing how to read from
      // the serial port. It must be changed in order to respect the
      // specifications of the protocol indicated in the Lab guide.

      // Read one byte from serial port
      // NOTE: This function may return even if no byte was received, which may
      // not be true.
      unsigned char byte;
      int bytes = readByteSerialPort(&byte);

      if (bytes < 0) {
        perror("readByteSerialPort");
        return -1;
      } else if (bytes == 0) {
        continue; // No byte received, try again
      }

      set_frame[nBytesBuf] = byte;
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
        if (byte == (set_frame[1] ^ set_frame[2])) {
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
  } else if (ll_config.role == LlRx) {
    unsigned char buf[SFRAME_SIZE] = {0};

    int nBytesBuf = 0;

    while (STOP == FALSE) {
      // Read one byte from serial port.
      // NOTE: You must check how many bytes were actually read by reading the
      // return value. In this example, we assume that the byte is always read,
      // which may not be true.
      unsigned char byte;
      int bytes = readByteSerialPort(&byte);

      if (bytes < 0) {
        perror("readByteSerialPort");
        return -1;
      } else if (bytes == 0) {
        continue; // No byte received, try again
      }

      buf[nBytesBuf] = byte;
      nBytesBuf += bytes;

      if (set_frame_state == START) {
        if (byte == FLAG) {
          set_frame_state = FLAG_RCV;
        }
      } else if (set_frame_state == FLAG_RCV) {
        if (byte == A_SENDER) {
          set_frame_state = A_RCV;
        } else {
          set_frame_state = START;
          STOP = TRUE;
        }
      } else if (set_frame_state == A_RCV) {
        if (byte == C_SET) {
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
        printf("SET frame received successfully\n");
        unsigned char ua_frame[SFRAME_SIZE] = {0};

        if (create_sframe(ua_frame) != SFRAME_SIZE) {
          return -1;
        }

        bytes = writeBytesSerialPort(ua_frame, 5);
        printf("UA frame sent\n");

        // Wait until all bytes have been written to the serial port
        sleep(1);
        STOP = TRUE;
      }
    }

    printf("Total bytes received: %d\n", nBytesBuf);
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
  // TODO: Implement this function
  // TODO: Dont send only data, put it in a format frame
  // TODO: the check bytesWritten must be changes since iframe will  have size
  // greater than bufSize
  // TODO: Byte stuffing?????

  int maxAttempts = ll_config.nRetransmissions + 1;
  for (int attempts = 0; attempts < maxAttempts; attempts++) {
    int bytesSent = transmit_frame(buf, bufSize);
    if (bytesSent > 0) {
      // TODO (M4): wait for RR/REJ here
      // For M3, just return success
      return bufSize;
    }
  }
  return -1;

  // NOTE: PSEUDOCODE
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

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  // TODO: Implement this function

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
