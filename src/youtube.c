#include <stdio.h>

#define REGION_IMPLEMENTATION
#define REGION_STATIC
#define REGION_LINEAR
#include "region.h"

#define URL_IMPLEMENTATION
#include "url.h"

static Region region;

void *region_json_malloc(size_t bytes) {
  
  Region_Ptr ptr;
  if(!region_alloc(&ptr, &region, bytes)) {
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
    fprintf(stderr, "ERROR: ");\
    fprintf(stderr, __VA_ARGS__);		\
    fprintf(stderr, "\n");			\
    exit(1);					\
  }while(0)

int count = 0;

typedef struct{
    int active;
    Json_Parser *parser;
    FILE *f;

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
	  printf("%.*s\n", (int) size, data);
	  return false;
	} else if(ret == JSON_PARSER_RET_SUCCESS) {
	  context->done = true;
	}
      }

      fwrite(data, 1, size, context->f);
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

//youtube_on_elem_results_init
//youtube_on_object_elem_results_init

bool on_elem_json(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

  Json_Ctx *ctx = (Json_Ctx *) arg;

  if(type == JSON_PARSER_TYPE_STRING) {

    if(!string_alloc2(&ctx->prev, ctx->temp, content, content_size))
      return false;
    
  }

  return true;
}

bool on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  Json_Ctx *ctx = (Json_Ctx *) arg;

  if(ctx->state == 0) {
    const char *videoId = "videoId";
    size_t videoId_len = strlen("videoId");
    if(videoId_len != key_size || (strncmp(key_data, videoId, videoId_len) != 0) ) return true;
    
    if(json_array_len(ctx->array.as.arrayval) == 0) {

      printf("push: "str_fmt"\n", str_arg(ctx->prev) );
      region_rewind(ctx->temp, ctx->snapshot);
      
      //arr_push(results->videoIds, &results->prev);
      ctx->state = 0;
    }
  } else if(ctx->state == 1) {
    /*
    if(!string_eq_cstr(key, "title")) return;
    if(string_eq_cstr(results->prev, "Nutzer haben auch gesehen")) return;
    arr_push(results->videoIds, &results->prev);
    results->state = 0;
    */
  } else if(ctx->state == 2) {
    /*
    if(!string_eq_cstr(key, "videoId")) return;
    if(results->videoId.len) return;
    results->videoId = results->prev;
    */
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

  if(!region_init(&region, 1024 * 1024 * 100))
    panic("region_init");

  Region temp;
  if(!region_init(&temp, 1024 * 10))
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

  Json array;
  if(!json_array_init(&array.as.arrayval))
    panic("json_array_init");
  
  Json_Ctx ctx = { .array=array, .state=0, .temp=&temp, .snapshot=region_current(&temp)};

  Json_Parser parser_json = json_parser(on_elem_json, on_object_elem_json, NULL, &ctx);
  
  Context context = {-1, &parser_json, f, false};
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
