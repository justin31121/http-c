#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_DYNAMIC
#include "../src/region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#include "../src/http.h"

#include "../src/common.h"

static size_t count  = 0;

Http_Ret region_callback(Region *region, const char *data, size_t size) {

  Region_Ptr ptr;
  if(!region_alloc(&ptr, region, size)) {
    fprintf(stderr, "ERROR allocaint memory in region\n");
    return false;
  }
  memcpy( region_deref(ptr), data, size);
  count += size;
  
  return HTTP_RET_CONTINUE;
}

int main() {

  Region region;
  if(!region_init(&region, 4096))
    panic("region_init");

  Http http;
  if(!http_init(&http, "www.example.com", HTTPS_PORT, true))
    panic("http_init");

  if(!http_request(&http, "/", "GET", NULL, 0,
		   (Http_Write_Callback) region_callback, &region,
		   "Connection: Close\r\n"))
    panic("http_request");

  http_free(&http);

  printf("%.*s", (int) count, (char *) region_base(region) );
  printf("count: %zu\n", count);

  return 0;
}
