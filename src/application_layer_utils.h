#ifndef _APPLICATION_LAYER_UTILS_H_
#define _APPLICATION_LAYER_UTILS_H_

// Data packet
#define C_DATA 2
#define DATA_HEADER_SIZE 3
// L2 L1 – number of octets (K) in the data field -> (K = 256 * L2 + L1)

// Control packet
#define C_START 1
#define C_END 3
#define T_SIZE 0
#define T_NAME 1
#define T_SIZE_BYTES 4

#define MAX_FILENAME_LEN 255 // standard max filename length
#define MAX_FILENAME_BUF (MAX_FILENAME_LEN + 1)
#define MAX_CTRL_PKT_SIZE                                                      \
  (1 + (2 + T_SIZE_BYTES) + (2 + MAX_FILENAME_LEN)) // = 1 + 6 + 257 = 264

#endif // _APPLICATION_LAYER_UTILS_H_
