#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_DYNAMIC
#include "../src/region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#include "../src/http.h"

#include "../src/common.h"

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

  Region region;
  if(!region_init(&region, 4096))
    panic("region_init");

  Http http;
  if(!http_init(hostname, HTTPS_PORT, true, &http))
    panic("http_init");

  Http_Request request;
  if(!http_request_from(&http, route, "GET", "Connection: Close\r\n", NULL, 0, &request))
    panic("http_request_from");

  Http_Header header;
  while(http_next_header(&request, &header)) {
    printf("%s:%s\n", header.key, header.value);
  }
  printf("\n");

  size_t count = 0;

  char *data;
  size_t data_len;
  while(http_next_body(&request, &data, &data_len)) {
    Region_Ptr ptr;
    if(!region_alloc(&ptr, &region, data_len)) {
      fprintf(stderr, "ERROR allocaint memory in region\n");
      return false;
    }
    memcpy( region_deref(ptr), data, data_len);
    count += data_len;
  }

  http_free(&http);

  printf("%.*s", (int) count, (char *) region_base(&region) );
  printf("count: %zu\n", count);

  return 0;
}
