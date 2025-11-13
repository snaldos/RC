/**
 * Minimal FTP client: parses URL, connects, logs in
 * Usage: ./ftp_client ftp://[user:pass@]host/path
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *default_user = "anonymous";
const char *default_pass = "anonymous@";

struct ftp_url {
  char user[64];
  char password[64];
  char host[128];
  char path[256];
  char filename[128]; // last part of path
};

// URL syntax, as described in RFC1738:
// ftp://[<user>:<password>@]<host>/<url-path>
int parse_url(char *url, struct ftp_url *info) {
  // 1. Confirm scheme
  if (strncmp(url, "ftp://", 6) != 0) {
    return -1;
  }
  url += 6; // skip "ftp://"

  // 2. Split user[:pass]@host/path
  char *at = strchr(url, '@');
  if (at) {
    // There's user and maybe password
    char userpass[128];
    strncpy(userpass, url, at - url);
    userpass[at - url] = '\0';
    url = at + 1; // move past '@'

    // Split user:pass
    char *colon = strchr(userpass, ':');
    if (colon) {
      *colon = '\0';
      strcpy(info->user, userpass);
      strcpy(info->password, colon + 1);
    } else {
      strcpy(info->user, userpass);
      strcpy(info->password, default_pass);
    }
  } else {
    // No credentials provided
    strcpy(info->user, default_user);
    strcpy(info->password, default_pass);
  }

  // 3. Now url points to host/path
  char *slash = strchr(url, '/');
  if (!slash) {
    return -1;
  }

  *slash = '\0'; // terminate host
  strcpy(info->host, url);
  strcpy(info->path, slash + 1);

  // 4. Extract filename from path
  char *last_slash = strrchr(info->path, '/');
  if (last_slash) {
    strcpy(info->filename, last_slash + 1);
  } else {
    strcpy(info->filename, info->path);
  }

  return 0;
}

int main(int argc, char *argv[]) {

  char *example_url =
      "ftp://arnaldo:lopes@ftp.netlab.fe.up.pt/pub/path/to/file.txt";
  // const char *example_url =
  //     "ftp://arnaldo@ftp.netlab.fe.up.pt/pub/path/to/file.txt";
  // char *example_url = "ftp://ftp.netlab.fe.up.pt/pub/path/to/file.txt";
  char url_buffer[512];
  strncpy(url_buffer, example_url, sizeof(url_buffer) - 1);
  url_buffer[sizeof(url_buffer) - 1] = '\0';

  struct ftp_url info;
  if (parse_url(url_buffer, &info) != 0) {
    printf("Error parsing URL\n");
    return -1;
  }

  printf("User: %s\n", info.user);
  printf("Password: %s\n", info.password);
  printf("Host: %s\n", info.host);
  printf("Path: %s\n", info.path);
  printf("Filename: %s\n", info.filename);

  return 0;
}
