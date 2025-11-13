/**
 * Minimal FTP client: parses URL, connects, logs in
 * Usage: ./ftp_client ftp://[user:pass@]host/path
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FTP_PORT 21
#define BUF_SIZE 1024

// Simple structure to hold parsed URL parts
struct ftp_url {
    char user[128];
    char pass[128];
    char host[256];
    char path[512];
};

// Parse ftp://[user:pass@]host/path into struct ftp_url
int parse_ftp_url(const char *url, struct ftp_url *out) {
    // Example: ftp://user:pass@host/path
    if (strncmp(url, "ftp://", 6) != 0) return -1;
    const char *p = url + 6;
    const char *at = strchr(p, '@');
    const char *slash = strchr(at ? at : p, '/');
    if (!slash) return -1;
    if (at) {
        const char *colon = strchr(p, ':');
        if (colon && colon < at) {
            size_t ulen = colon - p;
            size_t plen = at - colon - 1;
            strncpy(out->user, p, ulen); out->user[ulen] = 0;
            strncpy(out->pass, colon + 1, plen); out->pass[plen] = 0;
        } else {
            size_t ulen = at - p;
            strncpy(out->user, p, ulen); out->user[ulen] = 0;
            out->pass[0] = 0;
        }
        size_t hlen = slash - (at + 1);
        strncpy(out->host, at + 1, hlen); out->host[hlen] = 0;
    } else {
        out->user[0] = 0;
        out->pass[0] = 0;
        size_t hlen = slash - p;
        strncpy(out->host, p, hlen); out->host[hlen] = 0;
    }
    strncpy(out->path, slash + 1, sizeof(out->path) - 1);
    out->path[sizeof(out->path) - 1] = 0;
    return 0;
}

// Read a line from socket (FTP responses end with CRLF)
ssize_t read_line(int sockfd, char *buf, size_t maxlen) {
    size_t i = 0;
    char c;
    while (i < maxlen - 1) {
        ssize_t n = read(sockfd, &c, 1);
        if (n <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return i;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[user:pass@]host/path\n", argv[0]);
        return 1;
    }
    struct ftp_url url;
    if (parse_ftp_url(argv[1], &url) != 0) {
        fprintf(stderr, "Invalid FTP URL format.\n");
        return 1;
    }
    printf("Host: %s\nUser: %s\nPass: %s\nPath: %s\n", url.host, url.user, url.pass, url.path);

    // DNS resolution
    struct hostent *h = gethostbyname(url.host);
    if (!h) {
        herror("gethostbyname");
        return 1;
    }
    char *ip = inet_ntoa(*(struct in_addr *)h->h_addr);
    printf("Resolved IP: %s\n", ip);

    // Connect to FTP server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(FTP_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }
    printf("Connected to FTP server.\n");

    // Read server greeting
    char buf[BUF_SIZE];
    read_line(sockfd, buf, BUF_SIZE);
    printf("< %s", buf);

    // Send USER
    char usercmd[256];
    snprintf(usercmd, sizeof(usercmd), "USER %s\r\n", url.user[0] ? url.user : "anonymous");
    write(sockfd, usercmd, strlen(usercmd));
    read_line(sockfd, buf, BUF_SIZE);
    printf("< %s", buf);

    // Send PASS
    char passcmd[256];
    snprintf(passcmd, sizeof(passcmd), "PASS %s\r\n", url.pass[0] ? url.pass : "anonymous@");
    write(sockfd, passcmd, strlen(passcmd));
    read_line(sockfd, buf, BUF_SIZE);
    printf("< %s", buf);

    // ...existing code for next FTP steps (CWD, TYPE, PASV, RETR, etc.)

    close(sockfd);
    return 0;
}
