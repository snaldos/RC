// Link layer protocol implementation

#include "link_layer.h"
#include "link_layer_utils.h"
#include "serial_port.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

/*
    TODO: try to reduce code repetition, specially in state machines (ex: create
   a function that parses frames and stuff)
   ? decide if you should or not allow I frames where payload is 0 bytes
   TODO: think about migrating static functions to link_layer_utils.h and create
   a cpp
   ? maybe look at see if tweaking is possible for Vmin and Vtime
   ? Should we make some arrays size more or less strict in size

*/

static LinkLayer ll_config;
static int ll_opened = 0;

static unsigned char tx_cur_ns = 0;      // cur N(s) Tx sends
static unsigned char rx_expected_ns = 0; // expected N(s) for Rx to receive

static volatile int alarm_triggered = FALSE;
static volatile int alarm_count = 0;

// F | A | C | BCC1 | D1...DN | BCC2 | F
const unsigned int MAX_IFRAME_SIZE = MAX_PAYLOAD_SIZE + 6;
const unsigned int MAX_STUFFED_BODY_SIZE = 2 * (4 + MAX_PAYLOAD_SIZE);
const unsigned int MAX_STUFFED_IFRAME_SIZE = 2 + MAX_STUFFED_BODY_SIZE;

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

static int apply_byte_destuffing_until(const unsigned char *src, int len,
                                       unsigned char *dest,
                                       const int unstuffed_len) {

  // returns next position from where it stopped
  if (src == NULL || len < 0 || dest == NULL) {
    return -1;
  }

  int dest_len = 0;
  int next = 0;
  while (dest_len < unstuffed_len && next < len) {

    if (src[next] == ESCAPE_OCTET) {
      if (next + 1 >= len) {
        // if next = len - 1, src[next] = ESCAPE_OCTET shouldnt happen in
        // theory
        return -1;
      }
      dest[dest_len++] = src[next + 1] ^ XOR_OCTET;
      next += 2;
    } else {
      dest[dest_len++] = src[next];
      next++;
    }
  }
  return next;
}

static int apply_byte_destuffing(const unsigned char *src, int len,
                                 unsigned char *dest,
                                 const int max_unstuffed_len) {
  if (src == NULL || len < 0 || dest == NULL) {
    return -1;
  }

  int dest_len = 0;
  for (int i = 0; i < len;) {

    if (dest_len >= max_unstuffed_len) {
      // prevent overflow
      return -1;
    }

    if (src[i] == ESCAPE_OCTET) {
      if (i + 1 >= len) {
        // if i = len - 1, src[i] = ESCAPE_OCTET shouldnt happen in theory
        return -1;
      }
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

  // NOTE: Currently allowing data_size = 0
  if (data == NULL || frame == NULL || data_size < 0 ||
      data_size > MAX_PAYLOAD_SIZE) {
    return -1;
  }

  // Information Frame format: F | A | C | BCC1 | D1 ... DN | BCC2 | F

  int idx = 0;
  frame[idx++] = FLAG;
  frame[idx++] = A_SENDER;

  unsigned char C = (tx_cur_ns == 0) ? C_I0 : C_I1;
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

static int send_ack_frame(unsigned char control_field) {
  unsigned char frame[SFRAME_SIZE];
  frame[0] = FLAG;
  frame[1] = A_RECEIVER;
  frame[2] = control_field;
  frame[3] = frame[1] ^ frame[2];
  frame[4] = FLAG;

  return writeBytesSerialPort(frame, SFRAME_SIZE);
}

static int send_iframe(const unsigned char *buf, int buf_size) {

  unsigned char raw_frame[MAX_IFRAME_SIZE];
  int raw_len = create_iframe(buf, buf_size, raw_frame);
  if (raw_len < 0) {
    return -1;
  }

  // byte stuffing

  // exclude the two flags
  int body_len = raw_len - 2;
  unsigned char *body = &raw_frame[1];

  unsigned char stuffed_body[MAX_STUFFED_BODY_SIZE];
  int stuffed_body_len = apply_byte_stuffing(body, body_len, stuffed_body);
  if (stuffed_body_len < MIN_IFRAME_BODY_SIZE) {
    return -1;
  }

  unsigned char final_iframe[MAX_STUFFED_IFRAME_SIZE];
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

    // Wait until all bytes have been written to the serial port
    // sleep(1);

    // Tx tries to parse UA frame
    unsigned char ua_a;
    unsigned char ua_c;

    while (sframe_state != SUCCESS) {
      unsigned char byte;
      int bytes_read = readByteSerialPort(&byte);

      if (bytes_read < 0) {
        perror("readByteSerialPort");
        return -1;
      } else if (bytes_read == 0) {
        continue; // No byte received, try again
      }

      if (sframe_state == START) {
        // Ensure ua_a and ua_c are reset
        ua_a = 0;
        ua_c = 0;
        if (byte == FLAG) {
          printf("Tracking UA FRAME...\n");
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == FLAG_RCV) {
        if (byte == A_RECEIVER) {
          sframe_state = A_RCV;
          ua_a = byte;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == A_RCV) {
        if (byte == C_UA) {
          sframe_state = C_RCV;
          ua_c = byte;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == C_RCV) {
        if (byte == (ua_a ^ ua_c)) {
          sframe_state = BCC_OK;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
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

      if (sframe_state == SUCCESS) {
        printf("UA frame received successfully\n");
      }
    }
  } else if (ll_config.role == LlRx) {

    // Rx tries to parse SET frame
    unsigned char set_a;
    unsigned char set_c;

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
        // Ensure set_a and set_c are reset
        set_a = 0;
        set_c = 0;
        if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        }
      } else if (sframe_state == FLAG_RCV) {
        if (byte == A_SENDER) {
          sframe_state = A_RCV;
          set_a = byte;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == A_RCV) {
        if (byte == C_SET) {
          sframe_state = C_RCV;
          set_c = byte;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
        } else {
          sframe_state = START;
        }
      } else if (sframe_state == C_RCV) {
        if (byte == (set_a ^ set_c)) {
          sframe_state = BCC_OK;
        } else if (byte == FLAG) {
          sframe_state = FLAG_RCV;
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

        // Wait until all bytes have been written to the serial port
        // sleep(1);
      }
    }
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

  int max_attempts = ll_config.nRetransmissions;
  for (int attempts = 0; attempts < max_attempts; attempts++) {

    int bytes_sent = send_iframe(buf, bufSize);
    if (bytes_sent < 0) {
      return -1;
    }

    // Only after sending the I frame should we set the alarm
    alarm_triggered = FALSE;
    alarm(ll_config.timeout);

    enum SUPERVISION_STATE ack_frame_state = START;

    unsigned char ack_a;
    unsigned char ack_c;

    while (!alarm_triggered) {
      unsigned char byte;
      int bytes_read = readByteSerialPort(&byte);
      if (bytes_read < 0) {
        if (errno == EINTR) {
          // read was interrupted by the alarm signal, just continue
          if (alarm_triggered) {
            break;
          }
          continue;
        }
        alarm(0);
        perror("readByteSerialPort");
        return -1;
      } else if (bytes_read == 0) {
        continue;
      }

      // Tx tries to parse ACK frame

      if (ack_frame_state == START) {
        // Ensure ack_a and ack_c are reset
        ack_a = 0;
        ack_c = 0;
        if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
        }
      } else if (ack_frame_state == FLAG_RCV) {
        if (byte == A_RECEIVER) {
          ack_frame_state = A_RCV;
          ack_a = byte;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
        } else {
          ack_frame_state = START;
        }
      } else if (ack_frame_state == A_RCV) {
        // Check for RR or REJ
        if (byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 ||
            byte == C_REJ1) {
          ack_frame_state = C_RCV;
          ack_c = byte;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
        } else {
          ack_frame_state = START;
        }
      } else if (ack_frame_state == C_RCV) {
        if (byte == (ack_a ^ ack_c)) {
          ack_frame_state = BCC_OK;
        } else if (byte == FLAG) {
          ack_frame_state = FLAG_RCV;
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

      if (ack_frame_state == SUCCESS) {

        if (alarm_triggered) {
          printf("LL: Timeout occurred, retransmitting (attempt %d of %d)\n",
                 attempts + 1, max_attempts);
          break;
        }

        // Extract N(r) from control byte (rightmost bit)
        unsigned char nr = ack_c & 0x01;

        if ((ack_c == C_RR0) || (ack_c == C_RR1)) {
          unsigned char expected_rr_nr = (tx_cur_ns + 1) % 2;

          if (nr != expected_rr_nr) {
            // Ignore duplicate RRs because of duplicate iframe sent to Rx
            printf("LL: Received RR but with wrong N(r), expected %d\n",
                   expected_rr_nr);

            // Reset state machine to parse next frame
            ack_frame_state = START;
            ack_a = ack_c = 0;
            continue;
          }

          printf("LL: Received RR%d - Frame acknowledged\n", nr);
          tx_cur_ns = expected_rr_nr;
          alarm(0);
          return bufSize;

        } else if (ack_c == C_REJ0 || ack_c == C_REJ1) {

          unsigned char expected_rej_nr = tx_cur_ns;

          if (nr != expected_rej_nr) {
            // In theory this should not happen, but just in case
            printf("LL: Received REJ but with wrong N(r), expected %d\n",
                   expected_rej_nr);

            // Reset state machine to parse next frame
            ack_frame_state = START;
            ack_a = ack_c = 0;
            continue;
          }

          // Exit while loop to retransmit immediately
          printf("LL: Received REJ%d - Retransmitting immediately\n", nr);
          alarm(0);
          break;
        }
      }
    }
    // ensure timer is off before next attempt
    alarm(0);
  }

  printf("LL: Maximum retransmissions reached, giving up\n");
  alarm(0);
  return 0;
}
////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  if (!ll_opened || ll_config.role != LlRx || packet == NULL) {
    return -1;
  }

  enum SUPERVISION_STATE iframe_state = START;
  unsigned char stuffed_body[MAX_STUFFED_BODY_SIZE];
  int idx = 0;

  // Rx tries to parse I-frame

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
        // Repeated flag — stay in FLAG_RCV
        printf("Found repeated flag, still at start of stuffed FRAME...\n");
        continue;
      } else {
        iframe_state = DATA_RCV;
      }
    }

    if (iframe_state == DATA_RCV) {
      if (byte == FLAG) {
        iframe_state = SUCCESS;
        printf("end of stuffed FRAME...\n");
      } else if (idx < MAX_STUFFED_BODY_SIZE - 1) {
        stuffed_body[idx++] = byte;
      } else {
        printf("LL: Frame too long, restarting\n");
        iframe_state = START;
      }
    }
  }

  unsigned int max_stuffed_header_size =
      2 * HEADER_SIZE; // All the header was stuffed
  unsigned char destuffed_header[max_stuffed_header_size];

  int stuffed_header_len = apply_byte_destuffing_until(
      stuffed_body, idx, destuffed_header, HEADER_SIZE);

  if (stuffed_header_len < HEADER_SIZE) {
    return -1; // Header incomplete
  }

  unsigned char A = destuffed_header[0];
  unsigned char C = destuffed_header[1];
  unsigned char bcc1 = destuffed_header[2];
  unsigned char ns = C == C_I0 ? 0 : 1;

  // First handle unexpected header values
  if (C != C_I0 && C != C_I1) {
    printf("LL: C field error detected\n");
    // Header error → ignore frame (Slide 15)
  } else if (A != A_SENDER) {
    printf("LL: A field error detected\n");
    // Header error → ignore frame (Slide 15)
  } else if (bcc1 != (A ^ C)) {
    printf("LL: BCC1 error detected\n");
    // Header error → ignore frame (Slide 15)
  } else if (rx_expected_ns != ns) {
    // Unexpected N(s) means we received a duplicated frame (our RR got lost)
    // Unexpected N(s) takes precedence over BCC2 error

    // TO THINK ABOUT: couldnt i just ignore the duplicated frame instead of
    // sending RR? sending RR increases complexity
    printf(
        "LL: Unexpected N(s) received, expected %d but got %d. Sending RR%d\n",
        rx_expected_ns, ns, rx_expected_ns);
    // Send RR with expected_iframe_ns
    unsigned char rr_control = (rx_expected_ns == 0) ? C_RR0 : C_RR1;
    if (send_ack_frame(rr_control) < 0) {
      return -1;
    }
  } else {

    // no unexpected header values, destuff payload_bcc2

    // rest is payload + bcc2
    int stuffed_payload_bcc2_size = idx - stuffed_header_len;
    const int max_destuffed_payload_bcc2_size = MAX_PAYLOAD_SIZE + 1;

    unsigned char destuffed_payload_bcc2[max_destuffed_payload_bcc2_size];
    unsigned char *stuffed_payload_bcc2 = &stuffed_body[stuffed_header_len];

    int destuffed_len = apply_byte_destuffing(
        stuffed_payload_bcc2, stuffed_payload_bcc2_size, destuffed_payload_bcc2,
        max_destuffed_payload_bcc2_size);

    if (destuffed_len < MIN_IFRAME_BODY_SIZE - 3) {
      return -1; // Min: No Di's | BCC2
    }

    // body length without A C BCC1 and BCC2
    int payload_len = destuffed_len - 1;
    unsigned char received_bcc2 = destuffed_payload_bcc2[payload_len];
    unsigned char computed_bcc2 = 0;
    for (int i = 0; i < payload_len; i++) {
      computed_bcc2 ^= destuffed_payload_bcc2[i];
    }

    if (received_bcc2 != computed_bcc2) {
      printf("LL: BCC2 error detected, sending REJ%d\n", rx_expected_ns);
      // Send REJ with expected_iframe_ns
      unsigned char rej_control = (rx_expected_ns == 0) ? C_REJ0 : C_REJ1;
      if (send_ack_frame(rej_control) < 0) {
        return -1;
      }
    } else {
      printf("LL: I-frame received correctly, N(s)=%d, payload size=%d\n", ns,
             payload_len);
      memcpy(packet, destuffed_payload_bcc2, payload_len);
      rx_expected_ns = (rx_expected_ns + 1) % 2;
      send_ack_frame(rx_expected_ns == 0 ? C_RR0 : C_RR1);
      return payload_len;
    }
  }

  // packet not filled, meaning 0 length
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
