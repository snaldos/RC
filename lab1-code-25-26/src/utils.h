// Utils header.

#ifndef _UTILS_H_
#define _UTILS_H_

#define BUF_SIZE 256

#define FLAG 0x7e
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07

enum SUPERVISION_STATE { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, SUCCESS };

#endif // _UTILS_H_