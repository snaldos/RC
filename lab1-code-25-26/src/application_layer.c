// Application layer protocol implementation

#include "application_layer.h"
#include "application_layer_utils.h"
#include "link_layer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/*
   TODO: think about migrating static functions to application_layer_utils.h and
   create a cpp

*/

static void to_big_endian_uint32(uint32_t value,
                                 unsigned char out[T_SIZE_BYTES]) {
  // Most significant byte
  out[0] = (value >> 24) & 0xFF;
  out[1] = (value >> 16) & 0xFF;
  out[2] = (value >> 8) & 0xFF;
  // Least significant byte
  out[3] = value & 0xFF;
}

static int create_tlv(unsigned char *buf, unsigned char type, unsigned char len,
                      const unsigned char *value) {

  // len tells how many bytes value has
  if (buf == NULL || value == NULL || len == 0) {
    return -1;
  }

  // T
  buf[0] = type;
  // L
  buf[1] = len;
  // V
  memcpy(&buf[2], value, len);

  return 2 + len;
}

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

    // Get file size
    struct stat st;
    if (stat(filename, &st) != 0) {
      perror("stat");
      fclose(file);
      llclose();
      return;
    }
    uint32_t filesize = (uint32_t)st.st_size;

    // Build START packet
    unsigned char ctrl_pkt[MAX_CTRL_PKT_SIZE];
    int pos = 0;
    ctrl_pkt[pos++] = C_START;

    // TLV: file size
    unsigned char size_be[T_SIZE_BYTES];

    // Convert filesize to big-endian (seems to be the standard network byte
    // order)
    to_big_endian_uint32(filesize, size_be);
    pos += create_tlv(&ctrl_pkt[pos], T_SIZE, T_SIZE_BYTES, size_be);

    // TLV: file name
    int name_len = strlen(filename);
    pos += create_tlv(&ctrl_pkt[pos], T_NAME, name_len,
                      (const unsigned char *)filename);

    // Send start packet
    printf("Sending START packet for file: %s (%u bytes)\n", filename,
           filesize);
    if (llwrite(ctrl_pkt, pos) != pos) {
      fprintf(stderr, "Failed to send START packet\n");
      fclose(file);
      llclose();
      return;
    }

    // Send data packets

    // Leave room for C+L2+L1
    unsigned char file_buf[MAX_PAYLOAD_SIZE - DATA_HEADER_SIZE];
    unsigned char data_pkt[MAX_PAYLOAD_SIZE];

    while (!feof(file)) {
      size_t data_len = fread(file_buf, 1, sizeof(file_buf), file);
      if (data_len <= 0)
        break;

      data_pkt[0] = C_DATA;
      // L2
      data_pkt[1] = (data_len >> 8) & 0xFF;
      // L1
      data_pkt[2] = data_len & 0xFF;

      // Build data packet
      memcpy(&data_pkt[3], file_buf, data_len);

      int data_pkt_len = DATA_HEADER_SIZE + (int)data_len;

      if (llwrite(data_pkt, data_pkt_len) != data_pkt_len) {
        fprintf(stderr, "Failed to send DATA packet\n");
        fclose(file);
        llclose();
        return;
      }
    }

    // Build and send end packet

    // "The control packet that signals the end of transmission (END) shall
    // repeat the information contained in the START packet"
    ctrl_pkt[0] = C_END;
    printf("Sending END packet\n");
    if (llwrite(ctrl_pkt, pos) != pos) {
      fprintf(stderr, "Failed to send END packet\n");
      fclose(file);
      llclose();
      return;
    }

    fclose(file);
    printf("File transmission completed\n");
  } else if (ll.role == LlRx) {

    FILE *file = NULL;
    unsigned char buf[MAX_PAYLOAD_SIZE];

    printf("Waiting for file...\n");

    while (1) {
      int len = llread(buf);
      if (len < 0) {
        fprintf(stderr, "Failed to read packet\n");
        if (file) {
          fclose(file);
        }
        llclose();
        return;
      } else if (len == 0) {
        continue;
      }

      unsigned char ctrl = buf[0];

      if (ctrl == C_START || ctrl == C_END) {
        // Process control packet

        if (len > MAX_CTRL_PKT_SIZE) {
          // Malformed control packet
          printf("Malformed control packet\n");
          continue;
        }

        // start from idx 1 to parse the TLVs that are after C
        int idx = 1;
        char recv_name[MAX_FILENAME_BUF] = {0};
        uint32_t recv_size = 0;

        while (idx < len) {
          if (idx + 2 > len) {
            // Malformed TLV, break
            break;
          }
          unsigned char t = buf[idx++];
          unsigned char l = buf[idx++];
          if (idx + l > len) {
            // Malformed TLV, break
            break;
          }

          if (t == T_SIZE) {
            if (l != T_SIZE_BYTES) {
              // Invalid size length
              break;
            }

            // Convert from big-endian to uint32_t where first byte is the msb
            recv_size = ((uint32_t)buf[idx] << 24) |
                        ((uint32_t)buf[idx + 1] << 16) |
                        ((uint32_t)buf[idx + 2] << 8) | (uint32_t)buf[idx + 3];
          } else if (t == T_NAME) {
            memcpy(recv_name, &buf[idx], l);
            recv_name[l] = '\0';
          }
          idx += l;
        }

        if (ctrl == C_START) {
          file = fopen(filename, "wb");
          if (!file) {
            perror("fopen");
            llclose();
            return;
          }
          printf("Receiving file: %s (%u bytes)\n", recv_name, recv_size);
        } else if (ctrl == C_END) {
          if (file) {
            fclose(file);
          }

          // Get file size
          struct stat st;
          if (stat(filename, &st) != 0) {
            perror("stat");
            fclose(file);
            llclose();
            return;
          }
          uint32_t filesize = (uint32_t)st.st_size;

          printf("File transfer complete: %s (%u bytes) -> %s (%u bytes)\n",
                 recv_name, recv_size, filename, filesize);
          break;
        }

      } else if (ctrl == C_DATA) {
        // Process data packet
        if (len < DATA_HEADER_SIZE || len > MAX_PAYLOAD_SIZE) {
          // Malformed DATA packet
          printf("Malformed DATA packet\n");
          continue;
        }

        // Reconstruct data length k from L2 and L1
        int k = (buf[1] << 8) | buf[2];
        if (len != DATA_HEADER_SIZE + k) {
          printf("Invalid DATA packet length\n");
          continue;
        }
        if (file) {
          fwrite(&buf[3], 1, k, file);

          // Optional: flush to ensure data is written immediately
          fflush(file);
        }
      } else {
        // Unknown packet type: ignore
        printf("Unknown packet type: %d\n", ctrl);
      }
    }
  }

  if (llclose() < 0) {
    fprintf(stderr, "Failed to close link layer\n");
    return;
  }
}
