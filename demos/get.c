#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_DYNAMIC
#include "../src/region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#include "../src/http.h"

#include "../src/common.h"

int main() {

  Region region;
  if(!region_init(&region, 4096))
    panic("region_init");

  Http http;
  if(!http_init("www.example.com", HTTPS_PORT, true, &http))
    panic("http_init");

  Http_Request request;
  if(!http_request_from(&http, "/", "GET", "Connection: Close\r\n", NULL, 0, &request))
    panic("http_request_from");

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
