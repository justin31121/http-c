#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "http.h"

#define REGION_IMPLEMENTATION
#include "region.h"

#define IO_IMPLEMENTATION
#include "io.h"

#define BASE64_IMPLEMENTATION
#include "base64.h"

#include "common.h"

typedef struct{
  Region_Ptr data;
  size_t len;
}string;

#define str_fmt "%.*s"
#define str_arg(s) (int) (s).len, region_linear_deref((s).data)

typedef bool (*string_map_func)(const char *input, size_t input_size, char *buffer, size_t buffer_size, size_t *output_size);

bool string_map(string *s, Region_Linear *region, string input, string_map_func map_func) {
  s->len = input.len;
  if(!region_linear_alloc(&s->data, region, s->len)) return false;
  size_t output_size;
  bool map_success = map_func( (const char *) region_linear_deref(input.data), input.len, (char *) region_linear_deref(s->data), s->len, &output_size);
  while(!map_success) {
    s->len *= 2;
    region_rewind(*region, s->data);
    if(!region_linear_alloc(&s->data, region, s->len)) return false;
    map_success = map_func( (const char *) region_linear_deref(input.data), input.len, (char *) region_linear_deref(s->data), s->len, &output_size);
  }  
  s->len = output_size;
  region_rewind(*region, s->data);
  if(!region_linear_alloc(&s->data, region, s->len)) return false;
  return true;
}

bool string_alloc(string *s, Region_Linear *region, const char *cstr) {
  s->len = strlen(cstr);
  if(!region_linear_alloc(&s->data, region, s->len)) return false;
  memcpy( region_linear_deref(s->data), cstr, s->len);
  return true;
}

bool string_snprintf(string *s, Region_Linear *region, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list args_copy = args;  
  s->len = vsnprintf(NULL, 0, fmt, args) + 1;
  va_end(args);
  if(!region_linear_alloc(&s->data, region, s->len)) return false;
  vsnprintf( (char *const) region_linear_deref(s->data), s->len, fmt, args_copy);
  return true;
}

bool region_callback(Region_Linear *region, const char *data, size_t size) {
  Region_Ptr ptr;
  if(!region_linear_alloc(&ptr, region, size)) return false;  
  memcpy( region_linear_deref(ptr), data, size);
  return true;
}

int main() {

  Region_Linear region;
  if(!region_linear_init(&region, 1))
    panic("region_init");

  char spotify_creds[1024];
  if(!io_getenv("SPOTIFY_CREDS", spotify_creds, sizeof(spotify_creds))) 
    panic("The envorinment variable 'SPOTIFY_CREDS' is not set");

  string creds;
  if(!string_alloc(&creds, &region, spotify_creds))
    panic("string_alloc");

  string creds_base64;  
  if(!string_map(&s, &region, creds, base64_encode))
    panic("string_map");

  string headers;
  if(!string_snprintf(&auth, &region,
		      "Authorization: Basic "str_fmt"\r\n"
		      "Content-Type: application/x-www-form-urlencoded\r\n"
		      "\0", str_arg(creds_base64) ))
    panic("string_snprintf");
  
  ////////////////////////////////////////

  Http http;
  if(!http_init(&http, "accounts.spotify.com", HTTPS_PORT, true))
    panic("http_init");

  Region_Ptr base = region_current(region);
  Http_Parser parser = http_parser(region_callback, NULL, &region);

  const char *body = "grant_type=client_credentials";  
  if(http_request(&http, "/api/token", "POST",
		  (const unsigned char *) body, (int) strlen(body),
		  (Http_Write_Callback) http_parser_consume, &parser,
		  (const char *) region_linear_deref(headers.data) ) != HTTP_RET_SUCCESS) {
    panic("http_request");
  }

  printf("%.*s", (int) parser.content_length, (char *) region_linear_deref(base) );
  
  return 0;
}
