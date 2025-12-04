#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* --------------- DOCUMENTATION --------------- */

/*
 * NOTE ON SOCKETS, IP, AND PORTS:
 *
 * A socket is a programming interface that allows communication between two
 * endpoints over a network. Each endpoint is defined by an IP address
 * (identifies the device on the network) and a port number (identifies the
 * specific service or application on that device). When you connect a socket to
 * an IP and port, you are telling the OS to send data to that device and
 * deliver it to the correct service. Example: Connecting to 192.168.1.10:21
 * means connecting to the FTP service (port 21) on the device with IP
 * 192.168.1.10. Multiple services can run on the same IP, each using a
 * different port. In FTP, the control connection uses port 21, while data
 * transfers may use other ports (especially in passive mode).
 *
 *
 *
 * USEFULL LINKS:
 *
 *
 * LAB 2 GUIDE, slide 24:43 TCP/IP and Application Protocols
 *
 * LAB 2 GUIDE, slide 44 - a example of the
 * sequence of commands and responses involved in a typical FTP session.
 *
 * LAB 2 PRESEENTATION, slide 15:24 - everything we need (berkley sockets API,
 * Client-server model, etc)
 *
 */

/* ------------------ DEFINES ------------------ */

#define FTP_CONTROL_PORT                                                       \
  21                     // Default port for FTP control connection (commands)
#define FTP_DATA_PORT 20 // Default port for FTP data connection in active mode
#define MAX_BUF 1024

/* ------------------ STRUCT ------------------ */
typedef struct {
  char user[128];
  char pass[128];
  char host[256];
  char path[512]; // full path on server
  char filename[256];
  char dir[512]; // directory path (without filename)
  char ip[32];   // resolved IP address
} ftp_url;

/* ------------------ PARSE URL ------------------ */
int parse_url(const char *url, ftp_url *out) {
  strcpy(out->user, "anonymous");
  strcpy(out->pass, "anonymous");
  out->host[0] = out->path[0] = out->filename[0] = out->dir[0] = out->ip[0] =
      '\0';

  if (strncmp(url, "ftp://", 6) != 0) {
    fprintf(stderr, "URL must start with ftp://\n");
    return -1;
  }
  url += 6;

  const char *at = strchr(url, '@');
  if (at) {
    char userpass[256];
    size_t len = at - url;
    if (len >= sizeof(userpass))
      len = sizeof(userpass) - 1;
    strncpy(userpass, url, len);
    userpass[len] = '\0';
    const char *colon = strchr(userpass, ':');
    if (colon) {
      strncpy(out->user, userpass, colon - userpass);
      out->user[colon - userpass] = '\0';
      strncpy(out->pass, colon + 1, sizeof(out->pass) - 1);
      out->pass[sizeof(out->pass) - 1] = '\0';
    } else {
      strncpy(out->user, userpass, sizeof(out->user) - 1);
      out->user[sizeof(out->user) - 1] = '\0';
    }
    url = at + 1;
  }

  const char *slash = strchr(url, '/');
  if (slash) {
    size_t hlen = slash - url;
    if (hlen >= sizeof(out->host))
      hlen = sizeof(out->host) - 1;
    strncpy(out->host, url, hlen);
    out->host[hlen] = '\0';
    strncpy(out->path, slash + 1, sizeof(out->path) - 1);
    out->path[sizeof(out->path) - 1] = '\0';
  } else {
    strncpy(out->host, url, sizeof(out->host) - 1);
    out->host[sizeof(out->host) - 1] = '\0';
    out->path[0] = '\0';
  }

  if (out->path[0]) {
    const char *last_slash = strrchr(out->path, '/');
    if (last_slash) {
      // Directory part
      size_t dirlen = last_slash - out->path;
      if (dirlen >= sizeof(out->dir))
        dirlen = sizeof(out->dir) - 1;
      strncpy(out->dir, out->path, dirlen);
      out->dir[dirlen] = '\0';
      // Filename part
      strncpy(out->filename, last_slash + 1, sizeof(out->filename) - 1);
      out->filename[sizeof(out->filename) - 1] = '\0';
    } else {
      out->dir[0] = '\0';
      strncpy(out->filename, out->path, sizeof(out->filename) - 1);
      out->filename[sizeof(out->filename) - 1] = '\0';
    }
  } else {
    out->dir[0] = '\0';
    strcpy(out->filename, "default.txt"); // fallback filename
  }

  return 0;
}

/* ------------------ SOCKET CONNECT ------------------ */
/*
 * Creates a TCP socket, sets up the address, and connects to the given IP and
 * port. Returns the socket fd or - 1 on error.
 */

int ftp_connect(const char *ip, int port) {
  int sockfd;
  struct sockaddr_in addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr.sin_addr);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    return -1;
  }
  return sockfd;
}

/* ------------------ READ SERVER RESPONSE ------------------ */
/*
 * Reads a reply from the FTP server.
 * - recv() blocks until data is available, the connection is closed, or an
 * error occurs.
 * - If n == 0: the server closed the connection (EOF).
 * - If n < 0: a socket error occurred.
 * - The reply ends when a line starts with a 3-digit code followed by a space
 * (RFC959).
 * - Returns the reply code on success, or -1 on error or if no valid reply code
 * is found.
 */

int ftp_read_reply(int sockfd, char *buffer) {
  int total = 0, code = 0;
  while (1) {
    int n = recv(sockfd, buffer + total, MAX_BUF - 1 - total, 0);
    if (n <= 0) {
      buffer[0] = '\0';
      return -1;
    }
    total += n;
    buffer[total] = '\0';
    // Look for 3-digit code + space (end of reply)
    char *line = buffer;
    while (line < buffer + total) {
      if (isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
          line[3] == ' ') {
        code = atoi(line);
        printf("[SERVER] %s", buffer);
        return code;
      }
      char *next = strchr(line, '\n');
      if (!next)
        break;
      line = next + 1;
    }
  }
  buffer[0] = '\0';
  return -1; // No valid reply code found
}

/* ------------------ SEND COMMAND ------------------ */
int ftp_send_cmd(int sockfd, const char *fmt, ...) {
  char cmd[MAX_BUF];
  // va_list is a type to hold the variable arguments (va means variable
  // arguments)
  va_list ap;
  // va_start initializes the variable argument list, starting after 'fmt'
  va_start(ap, fmt);
  // vsnprintf formats the variable arguments into the cmd buffer, like printf
  vsnprintf(cmd, sizeof(cmd), fmt, ap);
  // va_end cleans up the variable argument list
  va_end(ap);
  int len = strlen(cmd);
  int sent = send(sockfd, cmd, len, 0);
  if (sent != len) {
    perror("send");
    return -1;
  }
  printf("[CLIENT] %s", cmd);
  return 0;
}

/* ------------------ PASSIVE MODE ------------------ */
int ftp_enter_passive(int sockfd, char *ip, int *port) {
  char buf[MAX_BUF];
  if (ftp_send_cmd(sockfd, "PASV\r\n") < 0)
    return -1;
  ftp_read_reply(sockfd, buf);
  char *p = strchr(buf, '(');
  if (!p) {
    fprintf(stderr, "PASV parse error\n");
    return -1;
  }
  int h1, h2, h3, h4, p1, p2;
  if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
    fprintf(stderr, "PASV parse error\n");
    return -1;
  }
  snprintf(ip, 32, "%d.%d.%d.%d", h1, h2, h3, h4);
  *port = p1 * 256 + p2;
  printf("[DEBUG] PASV ip=%s port=%d\n", ip, *port);
  return 0;
}

/* ------------------ DOWNLOAD ------------------ */
int ftp_download(int data_sock, const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    perror("fopen");
    return -1;
  }
  char buf[MAX_BUF];
  int n;
  while ((n = recv(data_sock, buf, sizeof(buf), 0)) > 0)
    fwrite(buf, 1, n, f);
  fclose(f);
  printf("[INFO] Downloaded: %s\n", filename);
  return 0;
}

/* ------------------ MAIN ------------------ */
int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s ftp://...\n", argv[0]);
    return -1;
  }

  ftp_url url;
  if (parse_url(argv[1], &url) != 0) {
    fprintf(stderr, "URL parse failed\n");
    return -1;
  }

  struct hostent *h;
  if ((h = gethostbyname(url.host)) == NULL) {
    herror("gethostbyname");
    return -1;
  }
  strcpy(url.ip, inet_ntoa(*((struct in_addr *)h->h_addr)));

  printf("Connecting to %s (%s) as %s\n", url.host, url.ip, url.user);

  int control = ftp_connect(url.ip, FTP_CONTROL_PORT);
  if (control < 0) {
    fprintf(stderr, "Failed control connect\n");
    return -1;
  }

  char buf[MAX_BUF];
  int code = ftp_read_reply(control, buf);
  if (code == -1) {
    fprintf(stderr, "Error reading server reply (initial connect)\n");
    return -1;
  }
  if (code >= 400) {
    fprintf(stderr, "Server error %d\n", code);
    return -1;
  }

  // --- USER (login username) ---
  ftp_send_cmd(control, "USER %s\r\n", url.user);
  code = ftp_read_reply(control, buf);
  if (code == -1) {
    fprintf(stderr, "Error reading server reply (USER)\n");
    return -1;
  }
  if (code >= 400) {
    fprintf(stderr, "USER failed %d\n", code);
    return -1;
  }

  // --- PASS (login password) ---
  ftp_send_cmd(control, "PASS %s\r\n", url.pass);
  code = ftp_read_reply(control, buf);
  if (code == -1) {
    fprintf(stderr, "Error reading server reply (PASS)\n");
    return -1;
  }
  if (code >= 400) {
    fprintf(stderr, "PASS failed %d\n", code);
    return -1;
  }

  // --- PASV (enter passive mode) ---
  char pasv_ip[32];
  int pasv_port;
  if (ftp_enter_passive(control, pasv_ip, &pasv_port) < 0) {
    fprintf(stderr, "PASV failed\n");
    return -1;
  }
  int data = ftp_connect(pasv_ip, pasv_port);
  if (data < 0) {
    fprintf(stderr, "Data connect failed\n");
    return -1;
  }

  // --- RETR (retrieve file) ---
  ftp_send_cmd(control, "RETR %s\r\n", url.path);
  code = ftp_read_reply(control, buf);
  if (code == -1) {
    fprintf(stderr, "Error reading server reply (RETR)\n");
    return -1;
  }
  if (code >= 400) {
    fprintf(stderr, "RETR failed %d\n", code);
    return -1;
  }

  ftp_download(data, url.filename);
  close(data);

  // --- QUIT (close connection) ---
  ftp_send_cmd(control, "QUIT\r\n");
  code = ftp_read_reply(control, buf);
  if (code == -1) {
    fprintf(stderr, "Error reading server reply (QUIT)\n");
    // still close control and exit
  }
  close(control);

  return 0;
}
