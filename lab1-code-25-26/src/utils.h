// Utils header.

#ifndef _UTILS_H_
#define _UTILS_H_

// F | A | C | BCC1 | F
#define SFRAME_SIZE 5

// F | A | C | BCC1 | D1 .. DN | BCC2 | F
#define IFRAME_ADDED_SIZE 6

// Worst-case I-frame size after byte stuffing:
// Body = A(1) + C(1) + BCC1(1) + payload(MAX_PAYLOAD_SIZE) + BCC2(1) = 4 +
// MAX_PAYLOAD_SIZE After stuffing: up to 2*(4 + MAX_PAYLOAD_SIZE) Plus 2 FLAG
// bytes (unstuffed)
// = 2010 when MAX_PAYLOAD_SIZE = 1000

// Supervision (S) and Unnumbered (U) Frames

#define FLAG 0x7E

#define A_SENDER 0x03
#define A_RECEIVER 0x01

#define C_SET 0x03
#define C_UA 0x07
#define C_RR0 0xAA
#define C_RR1 0xAB
#define C_REJ0 0x54
#define C_REJ1 0x55
#define C_DISC 0x0B

// BCC1 A ^ C

// Information Frames (I)

// #define FLAG 0x7E

// #define A_SENDER 0x03
// #define A_RECEIVER 0x01

// N(s) and N(r) - STOP AND WAIT
#define C_0 0x00
#define C_1 0x80

// BCC1 A ^ C
// BCC2 D1 ^ D2 ^ D3 ... ^ ^ Dn

enum SUPERVISION_STATE { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, SUCCESS };

#endif // _UTILS_H_
