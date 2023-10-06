#include <stdio.h>

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#include "../src/http.h"

int main(int argc, char **argv) {

  if(argc < 2) {
    fprintf(stderr, "ERROR: Please provide enough arguments\n");
    fprintf(stderr, "USAGE: %s <url>\n", argv[0]);
    return 1;
  }

  const char *input = argv[1];

  size_t input_len = strlen(input);
  if(input_len<8 || strncmp(input, "https://", 8) != 0) {
    return false;
  }

  size_t n = 8;
  while(n < input_len && input[n] != '/') n++;
  if(n == input_len) {
    return 1;
  }

  char hostname[256];
  if(n > sizeof(hostname) - 1) {
    return 1;
  }
  memcpy(hostname, input + 8, n - 8);
  hostname[n - 8] = 0;

  const char *route = input + n;

  Http http;
  if(!http_init(hostname, HTTPS_PORT, true, &http)) {
    return 1;
  }

  Http_Request request;
  if(!http_request_from(&http, route, "HEAD", NULL, NULL, 0, &request)) {
    return 1;
  }

  Http_Header header;
  while(http_next_header(&request, &header)) {
    printf("%s:%s\n", header.key, header.value);
  }
  
  http_free(&http);

  return 0;
}
