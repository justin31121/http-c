#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_STATIC
#include "region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "http.h"

#define JSON_PARSER_IMPLEMENTATION
#include "json_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

#include "common.h"

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
    if(!http_init(&http, "dummyjson.com", HTTPS_PORT, true))
	panic("http_init");

    Json_Parser jparser = json_parser();
    jparser.on_elem = on_elem;
    jparser.on_object_elem = on_object_elem;
    jparser.on_array_elem = on_array_elem;
    jparser.arg = &region;
  
    Http_Parser parser = http_parser((Http_Parser_Write_Callback) json_parser_consume, NULL, &jparser);

    if(http_request(&http, "/products", "GET",
		    NULL, 0,
		    (Http_Write_Callback) http_parser_consume, &parser,
		    "Connection: Close\r\n") != HTTP_RET_SUCCESS)
	panic("http_request");

    http_free(&http);

    return 0;
}

