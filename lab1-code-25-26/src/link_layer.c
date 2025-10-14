// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "utils.h"

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
// Global sequence number for I-frames
static unsigned char next_seq_num = 0;

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

  unsigned char C = (next_seq_num == 0) ? C_0 : C_1;
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

static int transmit_frame(const unsigned char *buf, int buf_size) {

  const unsigned int max_iframe_size = (2 * (4 + MAX_PAYLOAD_SIZE) + 2);
  unsigned char raw_frame[max_iframe_size]; // you'll define this macro next
  int raw_len = create_iframe(buf, buf_size, raw_frame);
  if (raw_len < 0) {
    return -1;
  }

  // byte stuffing
  int body_len = raw_len - 2;
  unsigned char *body = &raw_frame[1];

  // note: in theory int max_stuffed_body_len = max_iframe_size - 2;
  unsigned char stuffed_body[max_iframe_size];
  int stuffed_body_len = apply_byte_stuffing(body, body_len, stuffed_body);
  if (stuffed_body_len < 0) {
    return -1;
  }

  // note: in theory int final_iframe_len = stuffed_body_len + 2;

  unsigned char final_iframe[max_iframe_size];
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

    while (STOP == FALSE) {
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

      set_frame[idx] = byte;
      idx += bytes_read;

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

    int idx = 0;

    while (STOP == FALSE) {
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

      buf[idx] = byte;
      idx += bytes_read;

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

        int bytes_written = writeBytesSerialPort(ua_frame, SFRAME_SIZE);

        if (bytes_written < 0) {
          return -1;
        }
        printf("UA frame sent\n");

        // TODO: validate the use of sleep
        // Wait until all bytes have been written to the serial port
        sleep(1);
        STOP = TRUE;
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
  // TODO: Implement this function

  int max_attempts = ll_config.nRetransmissions + 1;
  for (int attempts = 0; attempts < max_attempts; attempts++) {
    int bytes_sent = transmit_frame(buf, bufSize);
    if (bytes_sent > 0) {
      // TODO (M4): wait for RR/REJ here
      // For M3, just return success
      // TODO: understand if the return value should be bufSize or bytesSent ( i
      // think it should return bufSize in order to achieve layer independence)
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
