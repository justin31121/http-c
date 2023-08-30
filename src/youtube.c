#include <stdio.h>

#define REGION_IMPLEMENTATION
#define REGION_VERBOSE

#define REGION_STATIC
#define REGION_LINEAR
#include "region.h"

#define URL_IMPLEMENTATION
#include "url.h"

static Region region_json;

void *region_json_malloc(size_t bytes) {
  
  Region_Ptr ptr;
  if(!region_alloc(&ptr, &region_json, bytes)) {
    return NULL;
  }

  return region_deref(ptr);
}

#define JSON_IMPLEMENTATION
#define JSON_MALLOC region_json_malloc
#include "json.h"

#define JSON_PARSER_IMPLEMENTATION
#define JSON_PARSER_VERBOSE
#include "json_parser.h"

#define HTML_PARSER_IMPLEMENTATION
#define HTML_PARSER_VERBOSE
#include "html_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#define HTTP_PARSER_VERBOSE
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_VERBOSE
#include "http.h"

#define panic(...) do{				\
    fprintf(stderr, "ERROR: ");			\
    fprintf(stderr, __VA_ARGS__);		\
    fprintf(stderr, "\n");			\
    exit(1);					\
  }while(0)

int count = 0;

typedef struct{
  int active;
  Json_Parser *parser;

  bool done;
}Context;

typedef int64_t Node;

bool on_node(const char *name, void *arg, void **node) {

  Node n = (int64_t) count++;
  *node = (void *) n;
    
  return true;
}

bool on_node_content(void *node, const char *data, size_t size, void *arg) {

  static const char *needle = "var ytInitialData = ";
  size_t needle_len = strlen(needle);

  Context *context = arg;
  Node n = (Node) node;

  if(context->active == -1) {
    if(size >= needle_len && strncmp(needle, data, needle_len) == 0) {
      context->active = (int) n;
    }
    data += needle_len;
    size -= needle_len;
  }

  if((int) n == context->active) {

    if(!context->done) {	
      Json_Parser_Ret ret = json_parser_consume(context->parser, data, size);
      if(ret == JSON_PARSER_RET_ABORT) {
	return false;
      } else if(ret == JSON_PARSER_RET_SUCCESS) {
	context->done = true;
      }
    }
    
  }    
    
  return true;
}

typedef struct{
  Json array;
  string prev;
  int state;
  
  Region *temp;
  Region_Ptr snapshot;
}Json_Ctx;

bool on_elem_json(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

  Json_Ctx *ctx = (Json_Ctx *) arg;

  if(type == JSON_PARSER_TYPE_STRING) {

    if(!string_alloc2(&ctx->prev, ctx->temp, content, content_size)) {
      return false;
    }
  }

  *elem = (uint64_t *) type;

  return true;
}

bool on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  Json_Ctx *ctx = (Json_Ctx *) arg;

  Json_Parser_Type type = (uint64_t) elem;

  if(type != JSON_PARSER_TYPE_STRING) {
    region_rewind(ctx->temp, ctx->snapshot);
    return true;
  }

  if(ctx->state == 0) {
    const char *videoId = "videoId";
    size_t videoId_len = strlen(videoId);
    if(videoId_len == key_size &&
       (strncmp(key_data, videoId, videoId_len) == 0) ) {

      bool append = true;
      if(json_array_len(ctx->array.as.arrayval) > 1) {

	Json prev = json_array_get(ctx->array.as.arrayval, json_array_len(ctx->array.as.arrayval) - 2);

	size_t prev_len = strlen(prev.as.stringval);
	
	if(ctx->prev.len == prev_len &&
	   strncmp(region_deref(ctx->prev.data), prev.as.stringval, prev_len) == 0 ) {
	  append = false;
	}
        
      }

      if(append) {
	Json json;
	json.kind = JSON_KIND_STRING;
	if(!json_string_init2(&json.as.stringval, region_deref(ctx->prev.data), ctx->prev.len)) return false;
	if(!json_array_append(ctx->array.as.arrayval, &json));
	
	ctx->state = 1;
      }

    }     
  } else if(ctx->state == 1) {

    const char *text = "text";
    size_t text_len = strlen(text);

    if(text_len == key_size &&
       strncmp(text, key_data, key_size) == 0) {

      if(string_eq_cstr(ctx->prev, "Feedback senden")) {

	Json json;
	json.kind = JSON_KIND_STRING;
	if(!json_string_init(&json.as.stringval, "Unknown")) return false;
	if(!json_array_append(ctx->array.as.arrayval, &json));
	
	ctx->state = 0;
      } else {
	Json json;
	json.kind = JSON_KIND_STRING;
	if(!json_string_init2(&json.as.stringval, region_deref(ctx->prev.data), ctx->prev.len)) return false;
	if(!json_array_append(ctx->array.as.arrayval, &json));
      
	ctx->state = 0;
	
      }
    
    }

  }

  region_rewind(ctx->temp, ctx->snapshot);
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

    if(!region_init(&region_json, 1024 * 8))
      panic("region_init");

    Region region_temp;
    if(!region_init(&region_temp, 4000))
      panic("region_init");

    string input;
    if(!string_alloc(&input, &region_temp, input_cstr))
      panic("string_alloc");

    string input_encoded_url;
    if(!string_map(&input_encoded_url, &region_temp, input, url_encode))
      panic("string_map");

    string route;
    if(!string_snprintf(&route, &region_temp,
			"/results?search_query="str_fmt"\0",
			str_arg(input_encoded_url)))
      panic("string_snprintf");

    Http http;
    if(!http_init(&http, YOUTUBE_HOSTNAME, HTTPS_PORT, true))
      panic("http_init");

    Json array;
    if(!json_array_init(&array.as.arrayval))
      panic("json_array_init");
  
    Json_Ctx ctx = { .array=array, .state=0, .temp=&region_temp, .snapshot=region_current(&region_temp)};

    Json_Parser parser_json = json_parser(on_elem_json, on_object_elem_json, NULL, &ctx);
  
    Context context = {-1, &parser_json, false};
    Html_Parser parser_html = html_parser(on_node, NULL, NULL, on_node_content, &context);
  
    Http_Parser parser_http = http_parser((Http_Parser_Write_Callback) html_parser_consume, NULL, &parser_html);

    if(http_request(&http, region_deref(route.data), "GET",
		    NULL, -1,
		    (Http_Write_Callback) http_parser_consume, &parser_http,
		    "Connection: Close\r\n") != HTTP_RET_SUCCESS)
      panic("http_request");
  
    http_free(&http);

    //////////////////////////////////

    uint64_t len = json_array_len(array.as.arrayval);
    for(uint64_t i=0;i<14 && i<len;i++) {
      const char *elem = json_array_get(array.as.arrayval, i).as.stringval;

      if(i % 2 == 0) {
	printf("https://www.youtube.com/watch?v=%s\n", elem);
      } else {
	printf("%s\n", elem);
      }

    }
        
    return 0;
  }
