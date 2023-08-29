#include <stdio.h>

#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#include "region.h"

#define URL_IMPLEMENTATION
#include "url.h"

#define JSON_IMPLEMENTATION
#include "json.h"

#define JSON_PARSER_IMPLEMENTATION
#define JSON_PARSER_VERBOSE
#include "json_parser.h"

#define HTML_PARSER_IMPLEMENTATION
#define HTML_PARSER_BUFFER_CAP (1024 * 2)
#define HTML_PARSER_VERBOSE
#include "html_parser.h"

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

int count = 0;

typedef struct{
    int active;
    FILE *f;
}Context;

typedef int64_t Node;

bool on_node(const char *name, void *arg, void **node) {

    Node n = (int64_t) count++;
    *node = (void *) n;
    
    return true;
}

bool on_node_content(void *node, const char *data, size_t size, void *arg) {

    static const char *needle = "var ytInitialData = ";
    size_t needle_len = sizeof(needle) - 1;

    Context *context = arg;
    Node n = (Node) node;

    if(context->active == -1) {
	if(size >= needle_len && strncmp(needle, data, needle_len) == 0) {
	    context->active = (int) n;
	}
    }

    if((int) n == context->active) {
      json_parser_consume(context->parser, data, size);
      //fwrite(data, 1, size, context->f);
    }    
    
    return true;
}

typedef struct{
    Json *json;
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
	if(!json_string_init2((char **) &json->as.stringval, content, content_size)) return false;
    } break;
    case JSON_PARSER_TYPE_NUMBER: {
	double num = strtod(content, NULL);
	*json = json_number(num);
    } break;
    case JSON_PARSER_TYPE_ARRAY: {
	json->kind = JSON_KIND_ARRAY;
	if(!json_array_init(&json->as.arrayval)) return false;        
    } break;
    case JSON_PARSER_TYPE_FALSE: {
	*json = json_false();
    } break;
    case JSON_PARSER_TYPE_TRUE: {
	*json = json_true();
    } break;
    case JSON_PARSER_TYPE_NULL: {
	*json = json_null();
    } break;
    default: {
	printf("INFO: unexpected type: %s\n", json_parser_type_name(type) );
	return false;
    }
    }

    if(!ctx->got_root) {
	ctx->json = json;
	ctx->got_root = true;
    }

    *elem = json;

    return true;
}

bool on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
    (void) arg;
    //Json_Ctx *ctx = (Json_Ctx *) arg;

    Json *json = (Json *) object;
    Json *smol = (Json *) elem;
  
    if(!json_object_append2(json->as.objectval, key_data, key_size, smol)) {
	return false;
    }

    return true;
}

bool on_array_elem_json(void *array, void *elem, void *arg) {
    (void) arg;

    Json *json = (Json *) array;
    Json *smol = (Json *) elem;
  
    if(!json_array_append(json->as.arrayval, smol)) {
	return false;
    }

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

  FILE *f = fopen("rsc\\initialData.json", "wb");
  if(!f)
      panic("fopen");
  
  Context context = {-1, f};
  Html_Parser parser_html = html_parser(on_node, NULL, NULL, on_node_content, &context);
  
  Http_Parser parser_http = http_parser((Http_Parser_Write_Callback) html_parser_consume, NULL, &parser_html);

  if(http_request(&http, region_deref(route.data), "GET",
		   NULL, -1,
		   (Http_Write_Callback) http_parser_consume, &parser_http,
		   "Connection: Close\r\n") != HTTP_RET_SUCCESS)
    panic("http_request");

  fclose(f);
  
  http_free(&http);
  
  return 0;
}
