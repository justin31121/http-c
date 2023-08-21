#include <stdio.h>

#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#include "region.h"

#define URL_IMPLEMENTATION
#include "url.h"

#define JSON_PARSER_IMPLEMENTATION
#define JSON_PARSER_VERBOSE
#include "../src/json_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#define HTTP_PARSER_VERBOSE
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#define HTTP_VERBOSE
#include "http.h"

#define panic(...) do{				\
    fprintf(stderr, "ERROR: ");\
    fprintf(stderr, __VA_ARGS__);		\
    fprintf(stderr, "\n");			\
    exit(1);					\
  }while(0)

bool on_elem(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

  printf("parsed: %s\n", json_parser_type_name(type) );
  
  return true;
}

#define YOUTUBE_HOSTNAME "www.youtube.com"

int main(int argc, char **argv) {

  if(argc < 2) {
    fprintf(stderr, "ERROR: Please provide enough arguments\n");
    fprintf(stderr, "USAGE: %s <search-term>\n", argv[0]);
    return 1;
  }

  const char *input_cstr = argv[1];

  Region region;
  if(!region_init(&region, 1024))
    panic("region_init");

  string input;
  if(!string_alloc(&input, &region, input_cstr))
    panic("string_alloc");

  string input_encoded_url;
  if(!string_map(&input_encoded_url, &region, input, url_encode))
    panic("string_map");

  string route;
  if(!string_snprintf(&route, &region,
		      "/results?search_query="str_fmt"\0",
		      str_arg(input_encoded_url)))
    panic("string_snprintf");

  Http http;
  if(!http_init(&http, YOUTUBE_HOSTNAME, HTTPS_PORT, true))
    panic("http_init");

  FILE *f = fopen("rsc\\youtube.html", "wb");
  if(!f)
    panic("fopen");

  Http_Parser parser_http = http_parser((Http_Parser_Write_Callback) http_fwrite, NULL, f);

  if(http_request(&http, region_deref(route.data), "GET",
		   NULL, -1,
		   (Http_Write_Callback) http_parser_consume, &parser_http,
		   "Connection: Close\r\n") != HTTP_RET_SUCCESS)
    panic("http_request");

  fclose(f);
  
  http_free(&http);
  
  return 0;
}
