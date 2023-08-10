#include <stdio.h>

#define HTTP_PARSER_IMPLEMENTATION
#define HTTP_PARSER_VERBOSE
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
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
  Http_Parser parser;
  const char *error;
}Slot;

Http_Parser_Ret download_func_callback(void *userdata, const char *data, size_t size) {
  Slot *s = userdata;
  memcpy(s->space + s->off, data, size);
  s->off += size;
}

void *download_func(void *arg) {

  Slot *s = arg;

  Http http;
  if(!http_init(&http, hostname, HTTPS_PORT, true)) {
    s->error = "Can not establish connection in thread";
    return NULL;
  }

  s->parser = http_parser(download_func_callback, NULL, s);

  char buf[1024];
  if(snprintf(buf, sizeof(buf), "Connection: close\r\nRange: bytes=%lld-%lld\r\n",
	      s->off, s->off + s->size - 1) >= sizeof(buf)) {
    s->error = "buf overflow";
    return NULL;
  }

  if(http_request(&http, route, "GET",
		  NULL, -1,
		  (Http_Write_Callback) http_parser_consume, &s->parser, buf) != HTTP_RET_SUCCESS) {
    s->error = "Request failed in thread";
    return NULL;
  }

  http_free(&http);
  
  return NULL;
}

bool check_accept_range(void *userdata, Http_Parser_String key, Http_Parser_String value) {

  int *accept_range = userdata;

  if(http_parser_string_eq(key, "accept-ranges")) {
    *accept_range = 0;

    if(http_parser_string_eq(value, "bytes")) {
      *accept_range = 1;
    } else {
      return false;
    }
  }
  
  return true;
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

  if(argc < 2) {
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
  if(!http_init(&http, hostname, HTTPS_PORT, true)) {
    fprintf(stderr, "ERROR: Can not instantiate a connection\n");
    return 1;
  }

  //-1 :: not appeared
  // 0 :: appeared but not 'bytes'
  // 1 :: 'bytes'
  int accept_range = -1;
  Http_Parser parser = http_parser(NULL, check_accept_range, &accept_range);

  if(http_request(&http, route, "HEAD",
		  NULL, -1,
		  (Http_Write_Callback) http_parser_consume, &parser,
		  "Connection: close\r\n") != HTTP_RET_SUCCESS) {
    fprintf(stderr, "ERROR: The request failed\n");
    return 1;
  }

  if(accept_range != 1) {
    fprintf(stderr, "ERROR: Resource does not support Accept-Range\n");
    return 1;
  }

  //printf("content-length: %d\n", parser.content_length);

  unsigned char *space = malloc(parser.content_length);
  if(!space) {
    fprintf(stderr, "ERROR: Can not allocate enough memory\n");
    return 1;
  }

  Thread thread_ids[NUMBER_OF_THREADS];
  Slot slots[NUMBER_OF_THREADS];

  int64_t ds = parser.content_length / NUMBER_OF_THREADS;
 
  for(int i=0;i<NUMBER_OF_THREADS;i++) {

    int64_t start = ds * i;
    int64_t end = ds * (i + 1);
    if(i == NUMBER_OF_THREADS - 1) {
      end = parser.content_length;
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

  fwrite(space, parser.content_length, 1, f);

  fclose(f);
  
  http_free(&http);
  
  return 0;
}
