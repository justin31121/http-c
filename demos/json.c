#include <stdio.h>

#include "../src/common.h"

#define JSON_IMPLEMENTATION
#include "../src/json.h"

int main() {

  Json null = { .kind = JSON_KIND_NULL };
  Json valse = { .kind = JSON_KIND_FALSE };
  Json sixtynine = { .kind = JSON_KIND_NUMBER, .as.doubleval = 69.0 };
  Json string = { .kind = JSON_KIND_STRING };
  if(!json_string_init((char **) &string.as.stringval, "Hello from Json!")) panic("json_string_init");
  Json smoll = { .kind = JSON_KIND_ARRAY };
  if(!json_array_init(&smoll.as.arrayval)) panic("json_array_init");
  Json text = { .kind = JSON_KIND_STRING };
  if(!json_string_init((char **) &text.as.stringval, "This text is inside of an array")) panic("json_string_init");
  if(!json_array_append(smoll.as.arrayval, &text)) panic("json_array_append");
  Json fourtwenty = { .kind = JSON_KIND_NUMBER, .as.doubleval = 420.0 };
  if(!json_array_append(smoll.as.arrayval, &fourtwenty)) panic("json_array_append");
  Json object = { .kind = JSON_KIND_OBJECT };
  if(!json_object_init(&object.as.objectval)) panic("json_object_init");
  if(!json_object_append(object.as.objectval, "value", &null)) panic("json_object_append");
  if(!json_object_append(object.as.objectval, "second value", &sixtynine)) panic("json_object_append");
    
  Json array = { .kind = JSON_KIND_ARRAY };
  if(!json_array_init(&array.as.arrayval)) panic("json_array_init");

  int N = 20;
    
  for(int i=0;i<N;i++) {
    Json *json;
    switch(i % 6) {
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
    case 4: {
      json = &smoll;
    } break;
    case 5: {
      json = &object;
    } break;
    default: {
      continue;
    }
    }

    if(!json_array_append(array.as.arrayval, json)) panic("json_array_append");
  }

  printf( "Array: %llu\n", json_array_len(array) );
  json_fprint(stdout, array); printf("\n\n");

  printf( "Json: %zu\n", json_object_len(object) );
  json_fprint(stdout, object); printf("\n\n");

  if(json_object_has(object.as.objectval, "value")) {
    printf("'value': "); json_fprint( stdout, json_object_get(object.as.objectval, "value") ); printf("\n");
  } else {
    printf("'value' does not exist\n");
  }

  if(json_object_has(object.as.objectval, "foo")) {
    printf("'foo': "); json_fprint( stdout, json_object_get(object.as.objectval, "foo") ); printf("\n");
  } else {
    printf("'foo' does not exist\n");
  }

  
    
  return 0;
}
