#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_STATIC
#include "../src/region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_OPEN_SSL
#include "../src/http.h"

#define JSON_PARSER_IMPLEMENTATION
#include "../src/json_parser.h"

#include "../src/common.h"

static string string_root;

typedef struct{
    Json_Parser_Type type;
    string name;
    string content;
    size_t count;
}Json;

#define json_fmt "Json {'"str_fmt"': %s ('"str_fmt"') }"
#define json_arg(json) str_arg((json).name), json_parser_type_name((json).type), str_arg((json).content)

bool on_elem(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

    Region *region = (Region *) arg;

    string content_string;
    if(!string_alloc2(&content_string, region, content, content_size))
	return false;
    
    Json json = {.type = type, .name = string_root, .content = content_string, .count = 0 };
    
    Region_Ptr ptr;
    if(!region_alloc(&ptr, region, sizeof(Json))) return false;
    memcpy( region_deref(ptr), &json, sizeof(Json) );

    *elem = region_deref(ptr);
    
    //printf("allocated: "json_fmt"\n", json_arg(json)); fflush(stdout);
  
    return true;
}

bool on_object_elem(void *_object, const char *key_data, size_t key_size, void *_elem, void *arg) {

    if(!_elem || !_object)
	return false;

    Json *object = (Json *) _object;
    Json *elem = (Json *) _elem;
    
    Region *region = (Region *) arg;

    string key_string;
    if(!string_alloc2(&key_string, region, key_data, key_size))
	return false;
	
    elem->name = key_string;
    printf("OBJ: Appending "json_fmt" to "json_fmt"\n", json_arg(*elem), json_arg(*object)); fflush(stdout);

    return true;
}

bool on_array_elem(void *_array, void *_elem, void *arg) {
    if(!_array || !_elem) return false;

    Region *region = (Region *) arg;
        
    Json *array = (Json *) _array;
    Json *elem = (Json *) _elem;

    string name;
    if(!string_snprintf(&name, region, str_fmt"_%zu", str_arg(array->name), array->count++ ))
	return false;

    elem->name = name;
    
    printf("ARR: Appending "json_fmt" to "json_fmt"\n", json_arg(*elem), json_arg(*array));

    return true;
}

int main() {

    Region region;
    if(!region_init(&region, 1024 * 1024))
	panic("region_init");

    if(!string_alloc(&string_root, &region, "*root*"))
	panic("string_alloc");
    
    Http http;
    if(!http_init("dummyjson.com", HTTPS_PORT, true, &http))
	panic("http_init");

    Json_Parser jparser = json_parser_from(on_elem, on_object_elem, on_array_elem, &region);
  
    Http_Request request;
    if(!http_request_from(&http, "/products", "GET", NULL, NULL, 0, &request))
      panic("http_request_from");

    char *data;
    size_t data_len;
    while(http_next_body(&request, &data, &data_len)) {
      if(json_parser_consume(&jparser, data, data_len) != JSON_PARSER_RET_CONTINUE) {
	break;
      }
    }

    http_free(&http);

    return 0;
}

