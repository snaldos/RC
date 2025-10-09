// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "utils.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) {
        perror("openSerialPort");
        exit(-1);
    }

    printf("Serial port %s opened\n", connectionParameters.serialPort);

    volatile int STOP = FALSE;
    enum SUPERVISION_STATE set_frame_state = START;

    if (connectionParameters.role == LlTx) {
        unsigned char buf[BUF_SIZE] = {0};

        buf[0] = FLAG;
        buf[1] = A_SENDER;
        buf[2] = C_SET;
        buf[3] = A_SENDER ^ C_SET;
        buf[4] = FLAG;

        // In non-canonical mode, '\n' does not end the writing.
        // Test this condition by placing a '\n' in the middle of the buffer.
        // The whole buffer must be sent even with the '\n'.

        int bytes = writeBytesSerialPort(buf, BUF_SIZE);
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
          int bytes = readByteSerialPort(&byte);
        
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
    }
    else if (connectionParameters.role == LlRx) {
        unsigned char buf[BUF_SIZE] = {0};

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
            exit(-1);
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
            unsigned char ua_frame[5] = {FLAG, A_RECEIVER, C_UA, A_RECEIVER ^ C_UA,
                                         FLAG};
            
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
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    if (closeSerialPort() < 0) {
        perror("closeSerialPort");
        exit(-1);
    }

    printf("Serial port closed\n");

    return 0;
}
