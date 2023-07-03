#include <stdio.h>
#include <stdlib.h>

#define JSON_PARSER_IMPLEMENTATION
#include "json_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "http.h"

#include "common.h"

bool on_elem(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {
  (void) type;
  (void) content;
  (void) content_size;
  (void) arg;
  (void) elem;

  printf("on_elem\n");
  
  return true;
}

void on_object_elem(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  (void) object;
  (void) key_data;
  (void) key_size;
  (void) elem;
  (void) arg;

  printf("on_object_elem");
}

void on_array_elem(void *array, void *elem, void *arg) {
  (void) array;
  (void) elem;
  (void) arg;

  printf("on_array_elem");
}

int main() {

  const char *json_cstr = "[ 1, \"yoyoyoyo\", { \"a\": 1} ]";
  printf("json: '%s'\n", json_cstr);

  Json_Parser jparser = json_parser();
  jparser.on_elem = on_elem;
  jparser.on_object_elem = on_object_elem;
  jparser.on_array_elem = on_array_elem;

  size_t json_cstr_len = strlen(json_cstr);
  const char *data = json_cstr;
  size_t window = 1;
  
  Json_Parser_Ret ret;
  do{
    ret = json_parser_consume(&jparser, data, window);
    data += window;
    json_cstr_len -= window;
    if(json_cstr_len < window) window = json_cstr_len;
    if(!json_cstr_len) break;
  }while(ret == JSON_PARSER_RET_CONTINUE);

  printf("consumed: '%.*s'\n", (int) strlen(json_cstr) - (int) json_cstr_len, json_cstr);
  
  if(ret == JSON_PARSER_RET_ABORT) {
    printf("abort\n");
    return 1;
  } else if(ret == JSON_PARSER_RET_CONTINUE) {
    printf("continue\n");
    return 1;
  } else if(ret == JSON_PARSER_RET_SUCCESS) {
    printf("success\n");
    return 0;
  }
  
  return 1;
}

/*
int main1() {
  
  Http http;
  if(!http_init(&http, "dummyjson.com", HTTPS_PORT, true))
    panic("http_init");

  Json_Parser jparser = json_parser();
  jparser.on_elem = on_elem;
  jparser.on_object_elem = on_object_elem;
  jparser.on_array_elem = on_array_elem;
  
  Http_Parser parser = http_parser((Http_Parser_Write_Callback) json_parser_consume, NULL, &jparser);

  const char *body = "Hello!";
  if(http_request(&http, "/products", "GET",
		  (const unsigned char *) body, (int) strlen(body),
		  (Http_Write_Callback) http_parser_consume, &parser,
		  "Connection: Close\r\n") == HTTP_RET_ABORT)
    panic("http_request");

  http_free(&http);

  return 0;
}
*/
