#define REGION_IMPLEMENTATION
#define REGION_STATIC
#include "region.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "http.h"

#define IO_IMPLEMENTATION
#include "io.h"

#define BASE64_IMPLEMENTATION
#include "base64.h"

#include "common.h"

Http_Parser_Ret region_callback(Region *region, const char *data, size_t size) {
  Region_Ptr ptr;
  if(!region_alloc(&ptr, region, size)) return HTTP_PARSER_RET_ABORT;  
  memcpy( region_deref(ptr), data, size);
  return HTTP_PARSER_RET_CONTINUE;
}

int main() {

  Region region;
  if(!region_init(&region, 1024))
    panic("region_init");

  char spotify_creds[1024];
  if(!io_getenv("SPOTIFY_CREDS", spotify_creds, sizeof(spotify_creds))) 
    panic("The envorinment variable 'SPOTIFY_CREDS' is not set");

  string creds;
  if(!string_alloc(&creds, &region, spotify_creds))
    panic("string_alloc");

  string creds_base64;
  if(!string_map(&creds_base64, &region, creds, base64_encode))
    panic("string_map");

  string headers;
  if(!string_snprintf(&headers, &region,
		      "Authorization: Basic "str_fmt"\r\n"
		      "Content-Type: application/x-www-form-urlencoded\r\n"
		      "\0", str_arg(creds_base64) ))
    panic("string_snprintf");
  
  ////////////////////////////////////////

  Http http;
  if(!http_init(&http, "accounts.spotify.com", HTTPS_PORT, true))
    panic("http_init");

  Region_Ptr base = region_current(region);
  Http_Parser parser = http_parser((Http_Parser_Write_Callback) region_callback, NULL, &region);

  const char *body = "grant_type=client_credentials";
  if(http_request(&http, "/api/token", "POST",
		  (const unsigned char *) body, (int) strlen(body),
		  (Http_Write_Callback) http_parser_consume, &parser,
		  (const char *) region_deref(headers.data) ) != HTTP_RET_SUCCESS) {
    panic("http_request");
  }

  printf("%.*s", (int) parser.content_length, (char *) region_deref(base) );
  
  return 0;
}
