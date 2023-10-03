#include <stdio.h>

#define HTTP_PARSER_IMPLEMENTATION
#define HTTP_PARSER_VERBOSE
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
//#define HTTP_WIN32_SSL
#define HTTP_OPEN_SSL
#include "http.h"

#define THREAD_IMPLEMENTATION
#include "thread.h"

#define NUMBER_OF_THREADS 64

char hostname[256] = {0};
const char *route = NULL;

typedef struct{
  unsigned char *space;
  int64_t off;
  int64_t size;
  const char *error;
}Slot;

size_t total_size = 0;

void *download_func(void *arg) {

  Slot *s = arg;

  Http http;
  if(!http_init(hostname, HTTPS_PORT, true, &http)) {
    s->error = "Can not establish connection in thread";
    return NULL;
  }

  char buf[1024];
  if(snprintf(buf, sizeof(buf), "Connection: close\r\nRange: bytes=%lld-%lld\r\n",
	      s->off, s->off + s->size - 1) >= sizeof(buf)) {
    s->error = "buf overflow";
    return NULL;
  }

  Http_Request request;
  if(!http_request_from(&http, route, "GET", buf, NULL, 0, &request)) {
    s->error = "Can not instantiate request in thread";
    return NULL;    
  }

  size_t got = 0;

  char *data;
  size_t data_len;
  while(http_next_body(&request, &data, &data_len)) {
    memcpy(s->space + s->off, data, data_len);
    s->off += data_len;
    got += data_len;
    total_size += data_len;
  }

  http_free(&http);
  
  return NULL;
}

bool parse_input(const char *input) {
  size_t input_len = strlen(input);
  if(input_len<8 || strncmp(input, "https://", 8) != 0) {
    return false;
  }

  size_t n = 8;
  while(n < input_len && input[n] != '/') n++;
  if(n == input_len) {
    return false;
  }

  if(n > sizeof(hostname) - 1) {
    return false;
  }
  memcpy(hostname, input + 8, n - 8);
  route = input + n;

  return true;
}

int main(int argc, char **argv) {

  if(argc < 3) {
    fprintf(stderr, "ERROR: Please provide enough arguments\n");
    fprintf(stderr, "USAGE: %s <url> <output>\n", argv[0]);
    return 1;
  }

  const char *input = argv[1];
  const char *output = argv[2];

  if(!parse_input(input)) {
    fprintf(stderr, "ERROR: Could not parse the url. Make sure it is correct\n");
    return 1;
  }

  Http http;
  if(!http_init(hostname, HTTPS_PORT, true, &http)) {
    fprintf(stderr, "ERROR: Can not instantiate a connection\n");
    return 1;
  }

  //-1 :: not appeared
  // 0 :: appeared but not 'bytes'
  // 1 :: 'bytes'

  int accept_range = -1;
  Http_Request request;
  if(!http_request_from(&http, route, "HEAD", NULL, NULL, 0, &request)) {
    fprintf(stderr, "ERROR: Can not instantiate request\n");
    return 1;
  }

  const char accept_cstr[] = "accept-ranges";
  const char bytes_cstr[] = "bytes";

  Http_Header header;
  while(http_next_header(&request, &header)) {
    
    if(http_header_eq(header.key, header.key_len, accept_cstr, sizeof(accept_cstr) - 1)) {
      accept_range = 0;

      if(http_header_eq(header.value, header.value_len, bytes_cstr, sizeof(bytes_cstr) - 1)) {
	accept_range = 1;
      }
    }
  }

  ////////////////////////////////////////////////////////////
  
  if(accept_range != 1) {
    fprintf(stderr, "ERROR: Resource does not support Accept-Range\n");
    return 1;
  }

  printf("content-length: %llu\n", request.content_length);

  unsigned char *space = malloc(request.content_length);
  if(!space) {
    fprintf(stderr, "ERROR: Can not allocate enough memory\n");
    return 1;
  }

  Thread thread_ids[NUMBER_OF_THREADS];
  Slot slots[NUMBER_OF_THREADS];

  int64_t ds = request.content_length / NUMBER_OF_THREADS;
 
  for(int i=0;i<NUMBER_OF_THREADS;i++) {

    int64_t start = ds * i;
    int64_t end = ds * (i + 1);
    if(i == NUMBER_OF_THREADS - 1) {
      end = request.content_length;
    }

    slots[i] = (Slot) { space, start, end - start, NULL };
    if(!thread_create(&thread_ids[i], download_func, &slots[i])) {
      fprintf(stderr, "ERROR: Can not spawn thread\n");
      return 1;
    }
    
  }
  
  for(int i=0;i<NUMBER_OF_THREADS;i++) {
    thread_join(thread_ids[i]);

    if(slots[i].error) {
      fprintf(stderr, "ERROR: %s\n", slots[i].error);
      return 1;
    }

  }

  FILE *f = fopen(output, "wb");
  if(!f) {
    fprintf(stderr, "ERROR: Can not open: '%s' for writing\n", output);
    fclose(f);
    return 1;
  }

  fwrite(space, request.content_length, 1, f);

  fclose(f);
  
  http_free(&http);
  
  return 0;
}
