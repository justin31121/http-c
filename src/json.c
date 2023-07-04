#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"

typedef enum{
    JSON_KIND_NONE = 0,
    JSON_KIND_NULL = 1,
    JSON_KIND_FALSE = 2,
    JSON_KIND_TRUE,
    JSON_KIND_NUMBER,
    JSON_KIND_STRING,
    JSON_KIND_ARRAY,
    JSON_KIND_OBJECT
}Json_Kind;

const char *json_kind_name(Json_Kind kind) {
    switch(kind) {
    case JSON_KIND_NONE: return "NONE";
    case JSON_KIND_NULL: return "NULL";
    case JSON_KIND_TRUE: return "TRUE";
    case JSON_KIND_FALSE: return "FALSE";
    case JSON_KIND_NUMBER: return "NUMBER";
    case JSON_KIND_STRING: return "STRING";
    case JSON_KIND_ARRAY: return "ARRAY";
    case JSON_KIND_OBJECT: return "OBJECT";
    default: return NULL;
    }
}

#define json_fmt "%s"
#define json_arg(json) json_kind_name((json).kind)

typedef struct{
    void *data;
    uint64_t len;
    uint64_t cap;
}Json_Array;

typedef Json_Array Json_Object;

typedef struct{
    Json_Kind kind;
    union{
	double doubleval;
	const char *stringval;
	Json_Array arrayval;
	Json_Object objectval;
    }as;
}Json;

bool json_object_init(Json_Object *object) {
    
    object->len  =  0;
    object->cap  = 16;
    object->data = malloc( object->cap * (sizeof(Json) + sizeof(const char *)) );
    if(!object->data) {
	return false;
    }

    return true;
}

void json_object_free(Json_Object *object) {
    free(object->data);
}

bool json_string_init(char **string, const char *cstr) {
    size_t cstr_len = strlen(cstr) + 1;
    *string = malloc(cstr_len);
    if(!(*string)) {
	return false;
    }
    memcpy(*string, cstr, cstr_len);

    return true;
}

void json_string_free(char *string) {
    free(string);
}
    
bool json_array_init(Json_Array *array) {
    
    array->len  =  0;
    array->cap  = 16;
    array->data = malloc( array->cap * sizeof(Json) );
    if(!array->data) {
	return false;
    }

    return true;
}

#define json_array_get(array, pos) (Json *) ((unsigned char *) (array)->data + sizeof(Json) * (pos));

bool json_array_append(Json_Array *array, Json *json) {
    if( array->len >= array->cap ) {
	array->cap *= 2;
	array->data = realloc(array->data, array->cap);
	if(!array->data) {
	    return false;
	}
    }
    void *ptr = (unsigned char *) array->data + sizeof(Json) * array->len++;
    memcpy(ptr, json, sizeof(Json));

    return true;
}

void json_array_free(Json_Array *array) {
    free(array->data);
}

int main() {

    Json null = { .kind = JSON_KIND_NULL };
    Json valse = { .kind = JSON_KIND_FALSE };
    Json sixtynine = { .kind = JSON_KIND_NUMBER, .as.doubleval = 69.0 };
    Json string = { .kind = JSON_KIND_STRING };
    if(!json_string_init((char **) &string.as.stringval, "Hello from Json!")) panic("json_string_init");

    Json array = { .kind = JSON_KIND_ARRAY };
    if(!json_array_init(&array.as.arrayval)) panic("json_array_init");
    
    for(int i=0;i<20;i++) {
	Json *json;
	switch(i % 4) {
	case 0: {
	    json = &null;
	} break;
	case 1: {
	    json = &valse;
	} break;
	case 2: {
	    json = &sixtynine;
	} break;
	case 3: {
	    json = &string;
	} break;
	}

	if(!json_array_append(&array.as.arrayval, json)) panic("json_array_append");
    }

    printf( json_fmt": %llu \n", json_arg(array), array.as.arrayval.len);

    for(int i=0;i<20;i++) {
	Json *json = json_array_get(&array.as.arrayval, i);
	switch(json->kind) {
	case JSON_KIND_NUMBER: {
	    printf( "%2f\n", json->as.doubleval);
	} break;
	case JSON_KIND_STRING: {
	    printf( "%s\n", json->as.stringval);
	} break;
	default: {
	    printf( json_fmt"\n", json_arg(*json) );
	} break;
	}
    }
    
    return 0;
}
