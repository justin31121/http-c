#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_DYNAMIC
#include "../src/region.h"

#define JSON_IMPLEMENTATION
#include "../src/json.h"

#define JSON_PARSER_IMPLEMENTATION
#include "../src/json_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "../src/http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "../src/http.h"


#define IO_IMPLEMENTATION
#include "../src/io.h"

#define BASE64_IMPLEMENTATION
#include "../src/base64.h"

#include "../src/common.h"

Http_Parser_Ret region_callback(Region *region, const char *data, size_t size) {
  Region_Ptr ptr;
  if(!region_alloc(&ptr, region, size)) return HTTP_PARSER_RET_ABORT;  
  memcpy( region_deref(ptr), data, size);
  return HTTP_PARSER_RET_CONTINUE;
}

typedef struct{
  Json json;
  bool got_root;
}Json_Ctx;

bool on_elem_json(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

  Json_Ctx *ctx = (Json_Ctx *) arg;
  
  Json *json = malloc(sizeof(Json));
  if(!json) return false;
  
  switch(type) {
  case JSON_PARSER_TYPE_OBJECT: {
    json->kind = JSON_KIND_OBJECT;
    if(!json_object_init(&json->as.objectval)) return false;    
  } break;
  case JSON_PARSER_TYPE_STRING: {
    json->kind = JSON_KIND_STRING;
    if(!json_string_init2(&json->as.stringval, content, content_size)) return false;
  } break;
  case JSON_PARSER_TYPE_NUMBER: {
    double num = strtod(content, NULL);
    *json = json_number(num);
  } break;
  default: {
    printf("INFO: unexpected type: %s\n", json_parser_type_name(type) );
    return false;
  }
  }

  if(!ctx->got_root) {
    ctx->json = *json;
    ctx->got_root = true;
  }

  *elem = json;

  return true;
}

bool on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  (void) arg;

  Json *json = (Json *) object;
  Json *smol = (Json *) elem;
  
  if(!json_object_append2(json->as.objectval, key_data, key_size, smol)) {
    return false;
  }

  return true;
}

typedef struct{
  const char *prev;
  size_t prev_size;

  Region *region;
  string out;
}Access_Token;

bool on_elem_access_token(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {
  (void) type;
  (void) elem;

  Access_Token *a = (Access_Token *) arg;
  
  a->prev = content;
  a->prev_size = content_size;

  return true;
}

bool on_object_elem_access_token(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  (void) elem;
  (void) object;

  Access_Token *a = (Access_Token *) arg;

  if(strncmp("access_token", key_data, key_size) == 0) {
    if(!string_alloc2(&a->out, a->region, a->prev, a->prev_size)) return false;
  }

  return true;
}

bool get_access_token(const char *spotify_creds, Region *region, string *access_token) {
  Region_Ptr current = region_current(*region);
  
  string creds;
  if(!string_alloc(&creds, region, spotify_creds))
    return false;

  string creds_base64;
  if(!string_map(&creds_base64, region, creds, base64_encode))
    return false;
  
  string headers;
  if(!string_snprintf(&headers, region,
		      "Authorization: Basic "str_fmt"\r\n"
		      "Content-Type: application/x-www-form-urlencoded\r\n"
		      "\0", str_arg(creds_base64) ))
    return false;
  
  ////////////////////////////////////////

  Http http;
  if(!http_init(&http, "accounts.spotify.com", HTTPS_PORT, true))
    return false;

  Access_Token ctx = { NULL, 0, region, (string) {0} };

  Json_Parser jparser = json_parser();
  jparser.on_elem = on_elem_access_token;
  jparser.on_object_elem = on_object_elem_access_token;
  jparser.arg = &ctx;
  Http_Parser parser =
    http_parser((Http_Parser_Write_Callback) json_parser_consume, NULL, &jparser);

  region_rewind(region, current);
  const char *body = "grant_type=client_credentials";
  if(http_request(&http, "/api/token", "POST",
		  (const unsigned char *) body, (int) strlen(body),
		  (Http_Write_Callback) http_parser_consume, &parser,
		  (const char *) region_deref(headers.data) ) != HTTP_RET_SUCCESS) {
    return false;
  }

  http_free(&http);

  region_rewind(region, current);
  string_copy(access_token, region, ctx.out);
    
  return true;
}

int main() {

  Region region;
  if(!region_init(&region, 1))
    panic("region_init");

  char spotify_creds[1024];
  if(!io_getenv("SPOTIFY_CREDS", spotify_creds, sizeof(spotify_creds))) 
    panic("The envorinment variable 'SPOTIFY_CREDS' is not set");

  string access_token;
  if(!get_access_token(spotify_creds, &region, &access_token))
    panic("get_access_token");
  
  printf( str_fmt, str_arg(access_token) );

  //////////////////////////////////////////////////////

  
  
  
  return 0;
}

