#ifndef JSON_H
#define JSON_H

#include <stdint.h>

#ifndef JSON_DEF
#  define JSON_DEF static inline
#endif //JSON_DEF

#ifdef JSON_IMPLEMENTATION
#  define JSON_PARSER_IMPLEMENTATION
#endif // JSON_IMPLEMENTATION

// json_parser.h

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

//https://www.json.org/json-de.html

#ifndef JSON_PARSER_DEF
#define JSON_PARSER_DEF static inline
#endif //JSON_PARSER_DEF

#ifndef JSON_PARSER_LOG
#  ifdef JSON_PARSER_QUIET
#    define JSON_PARSER_LOG(...)
#  else
#    include <stdio.h>
#    define JSON_PARSER_LOG(...) fprintf(stderr, "JSON_PARSER_LOG: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#  endif // JSON_PARSER_QUIET
#endif // JSON_PARSER_LOG

typedef enum{
  JSON_PARSER_CONST_TRUE = 0,
  JSON_PARSER_CONST_FALSE,
  JSON_PARSER_CONST_NULL,
  JSON_PARSER_CONST_COUNT
}Json_Parser_Const;

typedef enum{
  JSON_PARSER_RET_ABORT = 0,
  JSON_PARSER_RET_CONTINUE = 1,
  JSON_PARSER_RET_SUCCESS = 2,
}Json_Parser_Ret;

typedef enum{
  JSON_PARSER_STATE_IDLE = 0,

  // true, false, null
  JSON_PARSER_STATE_CONST,
  
  // object
  JSON_PARSER_STATE_OBJECT,
  JSON_PARSER_STATE_OBJECT_DOTS,
  JSON_PARSER_STATE_OBJECT_COMMA,
  JSON_PARSER_STATE_OBJECT_KEY,

  // array
  JSON_PARSER_STATE_ARRAY,
  JSON_PARSER_STATE_ARRAY_COMMA,

  // string
  JSON_PARSER_STATE_STRING,

  // number
  JSON_PARSER_STATE_NUMBER,
  JSON_PARSER_STATE_NUMBER_DOT,

  // \u1234
  JSON_PARSER_STATE_ESCAPED_UNICODE,

  // \\"
  JSON_PARSER_STATE_ESCAPED_CHAR,
  
}Json_Parser_State;

typedef enum{
  JSON_PARSER_TYPE_NULL,
  JSON_PARSER_TYPE_TRUE,
  JSON_PARSER_TYPE_FALSE,
  JSON_PARSER_TYPE_NUMBER,
  JSON_PARSER_TYPE_STRING,
  JSON_PARSER_TYPE_OBJECT,
  JSON_PARSER_TYPE_ARRAY
}Json_Parser_Type;

JSON_PARSER_DEF const char *json_parser_type_name(Json_Parser_Type type) {
  switch(type) {
  case JSON_PARSER_TYPE_NULL:
    return "JSON_PARSER_TYPE_NULL";
  case JSON_PARSER_TYPE_TRUE:
    return "JSON_PARSER_TYPE_TRUE";
  case JSON_PARSER_TYPE_FALSE:
    return "JSON_PARSER_TYPE_FALSE";
  case JSON_PARSER_TYPE_NUMBER:
    return "JSON_PARSER_TYPE_NUMBER";
  case JSON_PARSER_TYPE_STRING:
    return "JSON_PARSER_TYPE_STRING";
  case JSON_PARSER_TYPE_OBJECT:
    return "JSON_PARSER_TYPE_OBJECT";
  case JSON_PARSER_TYPE_ARRAY:
    return "JSON_PARSER_TYPE_ARRAY";
  default:
    return "UNKNOWN";	
  }
}

#ifndef JSON_PARSER_STACK_CAP
#  define JSON_PARSER_STACK_CAP 1024
#endif //JSON_PARSER_STACK_CAP

#ifndef JSON_PARSER_BUFFER_CAP
#  define JSON_PARSER_BUFFER_CAP 8192
#endif //JSON_PARSER_BUFFER_CAP

#define JSON_PARSER_BUFFER 0
#define JSON_PARSER_KEY_BUFFER 1

typedef bool (*Json_Parser_On_Elem)(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem);
typedef bool (*Json_Parser_On_Object_Elem)(void *object, const char *key_data, size_t key_size, void *elem, void *arg);
typedef bool (*Json_Parser_On_Array_Elem)(void *array, void *elem, void *arg);

typedef struct{
  Json_Parser_On_Elem on_elem;
  Json_Parser_On_Object_Elem on_object_elem;
  Json_Parser_On_Array_Elem on_array_elem;
  void *arg;

  Json_Parser_State state;

  Json_Parser_State stack[JSON_PARSER_STACK_CAP];
  size_t stack_size;

  void *parent_stack[JSON_PARSER_STACK_CAP];
  size_t parent_stack_size;

  char buffer[2][JSON_PARSER_BUFFER_CAP];
  size_t buffer_size[2];

  size_t konst_index;
  Json_Parser_Const konst;
}Json_Parser;

// Public
JSON_PARSER_DEF Json_Parser json_parser_from(Json_Parser_On_Elem on_elem, Json_Parser_On_Object_Elem on_object_elem, Json_Parser_On_Array_Elem on_array_elem, void *arg);
JSON_PARSER_DEF Json_Parser_Ret json_parser_consume(Json_Parser *parser, const char *data, size_t size);

// Private
JSON_PARSER_DEF bool json_parser_on_parent(Json_Parser *parser, void *elem);

#ifdef JSON_PARSER_IMPLEMENTATION

static char *json_parser_const_cstrs[JSON_PARSER_CONST_COUNT] = {
  [JSON_PARSER_CONST_TRUE] = "true",
  [JSON_PARSER_CONST_FALSE] = "false",
  [JSON_PARSER_CONST_NULL] = "null",
};

JSON_PARSER_DEF Json_Parser json_parser_from(Json_Parser_On_Elem on_elem, Json_Parser_On_Object_Elem on_object_elem, Json_Parser_On_Array_Elem on_array_elem, void *arg) {
  Json_Parser parser = {0};
  parser.state = JSON_PARSER_STATE_IDLE;
  parser.on_elem = on_elem;
  parser.on_object_elem = on_object_elem;
  parser.on_array_elem = on_array_elem;
  parser.stack_size = 0;
  parser.arg = arg;
  return parser;
}

JSON_PARSER_DEF Json_Parser_Ret json_parser_consume(Json_Parser *parser, const char *data, size_t size) {    
  size_t konst_len;

 consume:  
  switch(parser->state) {
  case JSON_PARSER_STATE_IDLE: {
    idle:
    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if(isspace(data[0])) {      
      data++;
      size--;
      if(size) goto idle;
    } else if(data[0] == '{') {

      assert(parser->parent_stack_size < JSON_PARSER_STACK_CAP);
      void **elem = &parser->parent_stack[parser->parent_stack_size];
      if(parser->on_elem) {
	if(!parser->on_elem(JSON_PARSER_TYPE_OBJECT, NULL, 0, parser->arg, elem)) {
	  JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	  return JSON_PARSER_RET_ABORT;
	}
      }
      if(!json_parser_on_parent(parser, *elem)) {
	return JSON_PARSER_RET_ABORT;
      }
      parser->parent_stack_size++;
	    
      parser->state = JSON_PARSER_STATE_OBJECT;
      data++;
      size--;
      if(size) goto consume;
    } else if(data[0] == '[') {

      assert(parser->parent_stack_size < JSON_PARSER_STACK_CAP);
      void **elem = &parser->parent_stack[parser->parent_stack_size];
      if(parser->on_elem) {
	if(!parser->on_elem(JSON_PARSER_TYPE_ARRAY, NULL, 0, parser->arg, elem)) {
	  JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	  return JSON_PARSER_RET_ABORT;
	}
      }
      if(!json_parser_on_parent(parser, *elem)) {
	return JSON_PARSER_RET_ABORT;
      }
      parser->parent_stack_size++;

	    
      parser->state = JSON_PARSER_STATE_ARRAY;
      data++;
      size--;
      if(size) goto consume;
    } else if(data[0] == '\"') {
      parser->buffer_size[JSON_PARSER_BUFFER] = 0;
      parser->state = JSON_PARSER_STATE_STRING;
      data++;
      size--;
      if(size) goto consume;
    } else if( isdigit(data[0]) ) {
      parser->buffer_size[JSON_PARSER_BUFFER] = 0;
      parser->state = JSON_PARSER_STATE_NUMBER;
      goto consume;      
    } else if( data[0] == '-' ) {
      parser->buffer_size[JSON_PARSER_BUFFER] = 0;
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];
      parser->state = JSON_PARSER_STATE_NUMBER;
      data++;
      size--;
      if(size) goto consume;
    } else if( data[0] == 't') {
      parser->state = JSON_PARSER_STATE_CONST;
      parser->konst = JSON_PARSER_CONST_TRUE;
      parser->konst_index = 1;      
      data++;
      size--;
      if(size) goto consume;
    } else if( data[0] == 'f') {
      parser->state = JSON_PARSER_STATE_CONST;
      parser->konst = JSON_PARSER_CONST_FALSE;
      parser->konst_index = 1;      
      data++;
      size--;
      if(size) goto consume;
    } else if( data[0] == 'n') {
      parser->state = JSON_PARSER_STATE_CONST;
      parser->konst = JSON_PARSER_CONST_NULL;
      parser->konst_index = 1;      
      data++;
      size--;
      if(size) goto consume;
    } else {
      JSON_PARSER_LOG("Expected JsonValue but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_OBJECT: {
    object:
    
    if(!size)
      return JSON_PARSER_RET_CONTINUE;
    
    if( isspace(data[0]) ) {
      data++;
      size--;
      if(size) goto object;
    } else if( data[0] == '\"') {
      assert(parser->stack_size < JSON_PARSER_STACK_CAP);
      parser->stack[parser->stack_size++] = JSON_PARSER_STATE_OBJECT_DOTS;

      parser->buffer_size[JSON_PARSER_BUFFER] = 0;
      parser->buffer_size[JSON_PARSER_KEY_BUFFER] = 0;
      parser->state = JSON_PARSER_STATE_STRING;

      data++;
      size--;
      if(size) goto consume;
    } else if(data[0] == '}') {

      parser->parent_stack_size--;
	    
      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];

	data++;
	size--;
	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
      
      return JSON_PARSER_RET_SUCCESS;
    } else {
      JSON_PARSER_LOG("Expected termination of JsonObject or a JsonString: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_OBJECT_DOTS: {
    object_dots:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0]) ) {
      data++;
      size--;
      if(size) goto object_dots;
    } else if( data[0] == ':' ) {
      assert(parser->stack_size < JSON_PARSER_STACK_CAP);
      parser->stack[parser->stack_size++] = JSON_PARSER_STATE_OBJECT_COMMA;
      
      parser->state = JSON_PARSER_STATE_IDLE;

      data++;
      size--;
      if(size) goto consume;
    } else {
      JSON_PARSER_LOG("Expected ':' between JsonString and JsonValue but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_OBJECT_COMMA: {
    object_comma:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0])) {
      data++;
      size--;
      if(size) goto object_comma;
    } else if( data[0] == ',' ) {
      parser->state = JSON_PARSER_STATE_OBJECT_KEY;
     
      data++;
      size--;
      if(size) goto consume;
    } else if( data[0] == '}') {

      parser->parent_stack_size--;
		    
      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];
	data++;
	size--;
	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
      
      return JSON_PARSER_RET_SUCCESS;
    } else {
      JSON_PARSER_LOG("Expected ',' or the termination of JsonObject but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }
    
    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_OBJECT_KEY: {
    object_key:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0]) ) {
      data++;
      size--;
      if(size) goto object_key;
    } else if( data[0] != '\"') {
      JSON_PARSER_LOG("Expected JsonString but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    } else {
      assert(parser->stack_size < JSON_PARSER_STACK_CAP);
      parser->stack[parser->stack_size++] = JSON_PARSER_STATE_OBJECT_DOTS;
      parser->buffer_size[JSON_PARSER_BUFFER] = 0;
      parser->buffer_size[JSON_PARSER_KEY_BUFFER] = 0;
      parser->state = JSON_PARSER_STATE_STRING;
      
      data++;
      size--;
      if(size) goto consume;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_ARRAY: {
    array:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0]) ) {
      data++;
      size--;
      if(size) goto array;
    } else if( data[0] == ']') {

      parser->parent_stack_size--;
		    
      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];
	data++;
	size--;
	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
	    
      return JSON_PARSER_RET_SUCCESS;
    } else {
      assert(parser->stack_size < JSON_PARSER_STACK_CAP);
      parser->stack[parser->stack_size++] = JSON_PARSER_STATE_ARRAY_COMMA;
      parser->state = JSON_PARSER_STATE_IDLE;
      
      if(size) goto consume;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_ARRAY_COMMA: {
    array_comma:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0]) ) {
      data++;
      size--;
      if(size) goto array_comma;
    } else if(data[0] == ',') {
      assert(parser->stack_size < JSON_PARSER_STACK_CAP);
      parser->stack[parser->stack_size++] = JSON_PARSER_STATE_ARRAY_COMMA;
      parser->state = JSON_PARSER_STATE_IDLE;
      
      data++;
      size--;
      if(size) goto consume;
    } else if(data[0] == ']') {

      parser->parent_stack_size--;
	    
      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];
	data++;
	size--;
	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }

      return JSON_PARSER_RET_SUCCESS;
    } else {
      JSON_PARSER_LOG("Expected ',' or the termination of JsonArray but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }
    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_NUMBER: {
    number:
    
    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isdigit(data[0]) ) {
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];	    
      data++;
      size--;
      if(size) goto number;
    } else if( data[0] == '.') {
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];
      parser->state = JSON_PARSER_STATE_NUMBER_DOT;
      data++;
      size--;
      if(size) goto consume;
    } else {

      void *elem = NULL;
      if(parser->on_elem) {
	if(!parser->on_elem(JSON_PARSER_TYPE_NUMBER, parser->buffer[JSON_PARSER_BUFFER], parser->buffer_size[JSON_PARSER_BUFFER], parser->arg, &elem)) {
	  JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	  return JSON_PARSER_RET_ABORT;
	}	
      }
      if(!json_parser_on_parent(parser, elem)) {
	return JSON_PARSER_RET_ABORT;
      }

      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];

	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
      
      return JSON_PARSER_RET_SUCCESS;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_NUMBER_DOT: {
    number_dot:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( isspace(data[0]) ) {
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];
      data++;
      size--;
      if(size) goto number_dot;
    } else if( isdigit(data[0])) {
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];
      data++;
      size--;
      if(size) goto number_dot;
    } else {

      void *elem = NULL;
      if(parser->on_elem) {
	if(!parser->on_elem(JSON_PARSER_TYPE_NUMBER, parser->buffer[JSON_PARSER_BUFFER], parser->buffer_size[JSON_PARSER_BUFFER], parser->arg, &elem)) {
	  JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	  return JSON_PARSER_RET_ABORT;
	}
      }
      if(!json_parser_on_parent(parser, elem)) {
	return JSON_PARSER_RET_ABORT;
      }

      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];

	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
      
      return JSON_PARSER_RET_SUCCESS;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_STRING: {
    _string:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if( data[0] == '\"') {

      void *elem = NULL;
      if(parser->on_elem ) {
	if(!parser->stack_size ||
	   (parser->stack[parser->stack_size-1] !=
	    JSON_PARSER_STATE_OBJECT_DOTS)) {
	  if(!parser->on_elem(JSON_PARSER_TYPE_STRING, parser->buffer[JSON_PARSER_BUFFER], parser->buffer_size[JSON_PARSER_BUFFER], parser->arg, &elem)) {
	    JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	    return JSON_PARSER_RET_ABORT;
	  }
	  
	}
      }
      if(!json_parser_on_parent(parser, elem)) {
	return JSON_PARSER_RET_ABORT;
      }
	    
      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];

	data++;
	size--;
	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
	    
      return JSON_PARSER_RET_SUCCESS;
    } else if(data[0] == '\\') {

      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];

      if(parser->stack_size &&
	 parser->stack[parser->stack_size-1] == JSON_PARSER_STATE_OBJECT_DOTS) {
	assert(parser->buffer_size[JSON_PARSER_KEY_BUFFER] < JSON_PARSER_BUFFER_CAP);
	parser->buffer[JSON_PARSER_KEY_BUFFER][parser->buffer_size[JSON_PARSER_KEY_BUFFER]++] = data[0];
      }
      
      data++;
      size--;

      parser->state = JSON_PARSER_STATE_ESCAPED_CHAR;

      if(size) goto consume;
      return JSON_PARSER_RET_CONTINUE;
    } else if( data[0] != '\\') {
	  
      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];

      if(parser->stack_size &&
	 parser->stack[parser->stack_size-1] == JSON_PARSER_STATE_OBJECT_DOTS) {
	assert(parser->buffer_size[JSON_PARSER_KEY_BUFFER] < JSON_PARSER_BUFFER_CAP);
	parser->buffer[JSON_PARSER_KEY_BUFFER][parser->buffer_size[JSON_PARSER_KEY_BUFFER]++] = data[0];
      }
	    
      data++;
      size--;
      if(size) goto _string;
    } else {
      JSON_PARSER_LOG("Expected termination of JsonString but found: '%c'", data[0]);
      return JSON_PARSER_RET_ABORT;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_CONST: {
    konst:
    konst_len = strlen(json_parser_const_cstrs[parser->konst]);

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if(parser->konst_index == konst_len) {

      void *elem = NULL;
      if(parser->on_elem) {
	Json_Parser_Type type;
	if(parser->konst == JSON_PARSER_CONST_TRUE) {
	  type = JSON_PARSER_TYPE_TRUE;
	} else if(parser->konst == JSON_PARSER_CONST_FALSE) {
	  type = JSON_PARSER_TYPE_FALSE;
	} else {
	  type = JSON_PARSER_TYPE_NULL;
	}

	if(!parser->on_elem(type, NULL, 0, parser->arg, &elem)) {
	  JSON_PARSER_LOG("Failure because 'on_elem' returned false");
	  return JSON_PARSER_RET_ABORT;
	}
      }
      if(!json_parser_on_parent(parser, elem)) {
	return JSON_PARSER_RET_ABORT;
      }

      if(parser->stack_size) {
	parser->state = parser->stack[parser->stack_size-- - 1];

	if(size) goto consume;
	return JSON_PARSER_RET_CONTINUE;
      }
	    
      return JSON_PARSER_RET_SUCCESS;
    }

    if( data[0] == json_parser_const_cstrs[parser->konst][parser->konst_index] ) {	    
      data++;
      size--;
      parser->konst_index++;
      if(size) goto konst;
    } else {
      JSON_PARSER_LOG("Expected 'true', 'false' or 'null'. The string was not terminated correctly with: '%c'.\n"
		      "       Correct would be '%c'.", data[0], json_parser_const_cstrs[parser->konst][parser->konst_index]);
      return JSON_PARSER_RET_ABORT;
    }

    return JSON_PARSER_RET_CONTINUE;
  } break;
  case JSON_PARSER_STATE_ESCAPED_UNICODE: {
    escaped_char:

    if(!size)
      return JSON_PARSER_RET_CONTINUE;

    if(parser->konst_index > 0) {

      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = data[0];

      if(parser->stack_size &&
	 parser->stack[parser->stack_size-1] == JSON_PARSER_STATE_OBJECT_DOTS) {
	assert(parser->buffer_size[JSON_PARSER_KEY_BUFFER] < JSON_PARSER_BUFFER_CAP);
	parser->buffer[JSON_PARSER_KEY_BUFFER][parser->buffer_size[JSON_PARSER_KEY_BUFFER]++] = data[0];
      }

      parser->konst_index--;
      data++;
      size--;

      if(size) goto escaped_char;
      return JSON_PARSER_RET_CONTINUE;
    } else {
      parser->state = JSON_PARSER_STATE_STRING;
      goto consume;
    }
    
  } break;
  case JSON_PARSER_STATE_ESCAPED_CHAR: {

    if(!size) return JSON_PARSER_RET_CONTINUE;
        
    char c = data[0];

    if(c == '\"') c = '\"';
    else if(c == '\\') c = '\\';
    else if(c == '/') c = '/';
    else if(c == 'b') c = '\b';
    else if(c == 'f') c = '\f';
    else if(c == 'n') c = '\n';
    else if(c == 'r') c = '\r';
    else if(c == 't') c = 't';
    else if(c == 'u') {
      parser->konst_index = 4;
      parser->state = JSON_PARSER_STATE_ESCAPED_UNICODE;

      assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = c;

      if(parser->stack_size &&
	 parser->stack[parser->stack_size-1] == JSON_PARSER_STATE_OBJECT_DOTS) {
	assert(parser->buffer_size[JSON_PARSER_KEY_BUFFER] < JSON_PARSER_BUFFER_CAP);
	parser->buffer[JSON_PARSER_KEY_BUFFER][parser->buffer_size[JSON_PARSER_KEY_BUFFER]++] = c;
      }
	
      data++;
      size--;

      if(size) goto consume;
      return JSON_PARSER_RET_CONTINUE;
    } else {
      JSON_PARSER_LOG("Escape-haracter: '%c' is not supported in JsonString", c);
      return JSON_PARSER_RET_ABORT;
    }

    assert(parser->buffer_size[JSON_PARSER_BUFFER] < JSON_PARSER_BUFFER_CAP);
    parser->buffer[JSON_PARSER_BUFFER][parser->buffer_size[JSON_PARSER_BUFFER]++] = c;

    if(parser->stack_size &&
       parser->stack[parser->stack_size-1] == JSON_PARSER_STATE_OBJECT_DOTS) {
      assert(parser->buffer_size[JSON_PARSER_KEY_BUFFER] < JSON_PARSER_BUFFER_CAP);
      parser->buffer[JSON_PARSER_KEY_BUFFER][parser->buffer_size[JSON_PARSER_KEY_BUFFER]++] = c;
    }

    parser->state = JSON_PARSER_STATE_STRING;
    data++;
    size--;
    if(size) goto consume;
    return JSON_PARSER_RET_CONTINUE;

  } break;
  default: {
    JSON_PARSER_LOG("unknown state in json_parser_consume");
    return JSON_PARSER_RET_ABORT;
  } break;
  }
}

JSON_PARSER_DEF bool json_parser_on_parent(Json_Parser *parser, void *elem) {
    
  void *parent = NULL;
  if(parser->parent_stack_size) {
    parent = parser->parent_stack[parser->parent_stack_size - 1];
  } 

  if(!parser->stack_size)
    return true;

  Json_Parser_State state = parser->stack[parser->stack_size - 1];
    
  if((state == JSON_PARSER_STATE_OBJECT ||
      //state == JSON_PARSER_STATE_OBJECT_DOTS ||
      state == JSON_PARSER_STATE_OBJECT_COMMA ||
      state == JSON_PARSER_STATE_OBJECT_KEY) &&
     parser->on_object_elem) {	
    if(!parser->on_object_elem(parent, parser->buffer[JSON_PARSER_KEY_BUFFER], parser->buffer_size[JSON_PARSER_KEY_BUFFER], elem, parser->arg)) {
      JSON_PARSER_LOG("Failure because 'on_object_elem' returned false");
      return false;
      
    }
	
  } else if( (state == JSON_PARSER_STATE_ARRAY ||
	      state == JSON_PARSER_STATE_ARRAY_COMMA) &&
	     parser->on_array_elem) {	
    if(!parser->on_array_elem(parent, elem, parser->arg)) {
      JSON_PARSER_LOG("Failure because 'on_array_elem' returned false");
      return false;      
    }
  }

  return true;
}

#endif //JSON_PARSER_IMPLEMENTATION

#endif //JSON_PARSER_H


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

#ifndef JSON_ARRAY_PAGE_CAP
#  define JSON_ARRAY_PAGE_CAP 32
#endif //JSON_ARAY_PAGE_CAP

struct Json_Array_Page {
  void *data;
  uint64_t len;

  struct Json_Array_Page *next;
};

typedef struct Json_Array_Page Json_Array_Page;

typedef struct{
  uint64_t len;
  Json_Array_Page *first;
  Json_Array_Page *current;
}Json_Array;

typedef void * Json_Object_t;

typedef struct{
  Json_Kind kind;
  union{
    double doubleval;
    const char *stringval;
    Json_Array *arrayval;
    Json_Object_t objectval; // Json_Object
  }as;
}Json;

// hashtable.h :

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

// Rewritten version of this hashtable: https://github.com/exebook/hashdict.c/tree/master

typedef enum{
  JSON_HASHTABLE_RET_ERROR = 0,
  JSON_HASHTABLE_RET_COLLISION,
  JSON_HASHTABLE_RET_SUCCESS,
}Json_Hashtable_Ret;

typedef bool (*Json_Hashtable_Func)(char *key, size_t key_len, Json *value, size_t index, void *userdata);

typedef struct Json_Hashtable_Key Json_Hashtable_Key;

struct Json_Hashtable_Key{
  Json_Hashtable_Key *next;
  char *key;
  size_t len;
  Json value;
};

JSON_DEF bool json_hashtable_key_init(Json_Hashtable_Key **k, const char *key, size_t key_len);
JSON_DEF void json_hashtable_key_free(Json_Hashtable_Key *k);

typedef struct{
  Json_Hashtable_Key **table;
  size_t len;
  size_t count;
  double growth_treshold;
  double growth_factor;
  Json *value;
}Json_Hashtable;

JSON_DEF bool json_hashtable_init(Json_Hashtable *ht, size_t initial_len);
JSON_DEF Json_Hashtable_Ret json_hashtable_add(Json_Hashtable *ht, const char *key, size_t key_len);
JSON_DEF bool json_hashtable_find(Json_Hashtable *ht, const char *key, size_t key_len);
JSON_DEF bool json_hashtable_resize(Json_Hashtable *ht, size_t new_len);
JSON_DEF void json_hashtable_reinsert_when_resizing(Json_Hashtable *ht, Json_Hashtable_Key *k2);
JSON_DEF void json_hashtable_for_each(Json_Hashtable *ht, Json_Hashtable_Func func, void *userdata);
JSON_DEF void json_hashtable_free(Json_Hashtable *ht);

typedef Json_Hashtable *Json_Object;

typedef struct{
  size_t count;
  FILE *f;
}Json_Object_Data;

typedef struct{
  Json json;
  Json array;
  bool got_root;

  Json_Parser parser;
}Json_Context;

// json.h

bool json_context_init(Json_Context *ctx);
Json_Parser_Ret json_context_consume(Json_Context *ctx, const char *data, size_t data_len);
void json_context_free(Json_Context *ctx);

JSON_DEF const char *json_kind_name(Json_Kind kind);

JSON_DEF bool json_object_fprint(char *key, size_t key_len, Json *json, size_t index, void *_userdata);
JSON_DEF bool json_object_init(Json_Object_t *__ht);
JSON_DEF bool json_object_append(Json_Object_t *_ht, const char *key, Json *json);
JSON_DEF bool json_object_append2(Json_Object_t *_ht, const char *key, size_t key_len, Json *json);
JSON_DEF bool json_object_has(Json_Object_t *_ht, const char *key);
JSON_DEF Json json_object_get(Json_Object_t *_ht, const char *key);
JSON_DEF void json_object_free(Json_Object_t *_ht);

JSON_DEF bool json_string_init(char **string, const char *cstr);
JSON_DEF bool json_string_init2(char **string, const char *cstr, size_t cstr_len);
JSON_DEF void json_string_free(char *string);

JSON_DEF bool json_array_init(Json_Array **array);
JSON_DEF bool json_array_append(Json_Array *array, Json *json);
JSON_DEF void json_array_free(Json_Array *array);
JSON_DEF Json json_array_get(Json_Array *array, uint64_t pos);

JSON_DEF void json_fprint(FILE *f, Json json);
JSON_DEF void json_free(Json json);

#define json_array_len(array) (array)->len
#define json_object_len(object) ((Json_Hashtable *) (object))->count

#define json_null() (Json) {.kind = JSON_KIND_NULL }
#define json_false() (Json) {.kind = JSON_KIND_FALSE }
#define json_true() (Json) {.kind = JSON_KIND_TRUE }
#define json_number(d) (Json) {.kind = JSON_KIND_NUMBER, .as.doubleval = d }

#ifdef JSON_IMPLEMENTATION

#include <stdio.h>

static inline void *json_malloc_stub(void *userdata, size_t bytes) {
  (void) userdata;
  return malloc(bytes);
}

static inline void json_free_stub(void *userdata, void *ptr) {
  (void) userdata;
  free(ptr);
}

void *json_allocator_userdata = NULL;
void* (*json_allocator_alloc)(void *userdata, size_t bytes) = json_malloc_stub;
void (*json_allocator_free)(void *userdata, void* ptr) = json_free_stub;

bool json_on_elem_json(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

  Json_Context *ctx = (Json_Context *) arg;
  
  Json json;
  
  switch(type) {
  case JSON_PARSER_TYPE_OBJECT: {
    json.kind = JSON_KIND_OBJECT;
    if(!json_object_init(&json.as.objectval)) return false;    
  } break;
  case JSON_PARSER_TYPE_STRING: {
    json.kind = JSON_KIND_STRING;
    if(!json_string_init2((char **) &json.as.stringval, content, content_size)) return false;
  } break;
  case JSON_PARSER_TYPE_NUMBER: {
    double num = strtod(content, NULL);
    json = json_number(num);
  } break;
  case JSON_PARSER_TYPE_ARRAY: {
    json.kind = JSON_KIND_ARRAY;
    if(!json_array_init(&json.as.arrayval)) return false;        
  } break;
  case JSON_PARSER_TYPE_FALSE: {
    json = json_false();
  } break;
  case JSON_PARSER_TYPE_TRUE: {
    json = json_true();
  } break;
  case JSON_PARSER_TYPE_NULL: {
    json = json_null();
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

  *elem = (void *) ctx->array.as.arrayval->len;

  if(!json_array_append(ctx->array.as.arrayval, &json)) {
    return false;
  }

  return true;
}

bool json_on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  
  Json_Context *ctx = (Json_Context *) arg;

  uint64_t json_index = *(uint64_t *) &object;
  uint64_t smol_index = *(uint64_t *) &elem;

  Json json = json_array_get(ctx->array.as.arrayval, json_index);
  Json smol = json_array_get(ctx->array.as.arrayval, smol_index);
  
  if(!json_object_append2(json.as.objectval, key_data, key_size, &smol)) {
    return false;
  }

  return true;
}

bool json_on_array_elem_json(void *array, void *elem, void *arg) {
  (void) arg;

  Json_Context *ctx = (Json_Context *) arg;

  uint64_t json_index = *(uint64_t *) &array;
  uint64_t smol_index = *(uint64_t *) &elem;

  Json json = json_array_get(ctx->array.as.arrayval, json_index);
  Json smol = json_array_get(ctx->array.as.arrayval, smol_index);
  
  if(!json_array_append(json.as.arrayval, &smol)) {
    return false;
  }

  return true;
}

bool json_context_init(Json_Context *ctx) {  
  ctx->got_root = false;

  ctx->array.kind = JSON_KIND_ARRAY;
  if(!json_array_init(&ctx->array.as.arrayval)) {
    return false;
  }
  
  ctx->parser = json_parser_from(json_on_elem_json, json_on_object_elem_json, json_on_array_elem_json, ctx);

  return true;
}

Json_Parser_Ret json_context_consume(Json_Context *ctx, const char *data, size_t data_len) {
  return json_parser_consume(&ctx->parser, data, data_len);
}

void json_context_free(Json_Context *ctx) {  
  json_free(ctx->json);
  json_array_free(ctx->array.as.arrayval);
}

JSON_DEF const char *json_kind_name(Json_Kind kind) {
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

JSON_DEF bool json_object_init(Json_Object_t *__ht) {

  Json_Hashtable **_ht = (Json_Hashtable **) __ht;
  
  Json_Hashtable *ht = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Hashtable));
  if(!ht) {
    return false;
  }

  if(!json_hashtable_init(ht, 0)) {
    return false;
  }

  *_ht = ht;

  return true;
}

JSON_DEF bool json_object_append(Json_Object_t *_ht, const char *key, Json *json) {

  Json_Hashtable *ht = (Json_Hashtable *) _ht;

  Json_Hashtable_Ret ret = json_hashtable_add(ht, key, strlen(key));
  if(ret == JSON_HASHTABLE_RET_ERROR) {
    return false;
  }
  *ht->value = *json;
  
  return true;
}

JSON_DEF bool json_object_append2(Json_Object_t *_ht, const char *key, size_t key_len, Json *json) {
  Json_Hashtable *ht = (Json_Hashtable *) _ht;

  Json_Hashtable_Ret ret = json_hashtable_add(ht, key, key_len);
  if(ret == JSON_HASHTABLE_RET_ERROR) {
    return false;
  }
  *ht->value = *json;
  
  return true;
  
}

JSON_DEF bool json_object_has(Json_Object_t *_ht, const char *key) {
  Json_Hashtable *ht = (Json_Hashtable *) _ht;
  return json_hashtable_find(ht, key, strlen(key));
}

JSON_DEF Json json_object_get(Json_Object_t *_ht, const char *key) {
  Json_Hashtable *ht = (Json_Hashtable *) _ht;
  json_hashtable_find(ht, key, strlen(key)); // no checking
  return *ht->value;
}

JSON_DEF void json_object_free(Json_Object_t *_ht) {
  json_hashtable_free((Json_Hashtable *) _ht);
}

JSON_DEF bool json_string_init(char **string, const char *cstr) {
  size_t cstr_len = strlen(cstr) + 1;
  *string = json_allocator_alloc(json_allocator_userdata, cstr_len);
  if(!(*string)) {
    return false;
  }
  memcpy(*string, cstr, cstr_len);

  return true;
}

JSON_DEF bool json_string_init2(char **string, const char *cstr, size_t cstr_len) {
  *string = json_allocator_alloc(json_allocator_userdata, cstr_len + 1);
  if(!(*string)) {
    return false;
  }
  memcpy(*string, cstr, cstr_len);
  (*string)[cstr_len] = 0;
  return true;  
}

JSON_DEF void json_string_free(char *string) {
  json_allocator_free(json_allocator_userdata, string);
}
    
JSON_DEF bool json_array_init(Json_Array **_array) {

  Json_Array *array = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Array));
  if(!array) {
    return false;
  }

  Json_Array_Page *page = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Array_Page));
  if(!page) {
    return false;
  }
  
  page->len = 0;
  page->next = NULL;
  page->data = json_allocator_alloc(json_allocator_userdata, JSON_ARRAY_PAGE_CAP * sizeof(Json) );
  if(!page->data) {
    return false;
  }

  array->len = 0;
  array->first = page;
  array->current = page;

  *_array = array;

  return true;
}

JSON_DEF bool json_array_append(Json_Array *array, Json *json) {

  Json_Array_Page *page = array->current;
  if(page->len >= JSON_ARRAY_PAGE_CAP) {
    //append new page

    Json_Array_Page *new_page = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Array_Page));
    if(!new_page) {
      return false;
    }
  
    new_page->len = 0;
    new_page->next = NULL;
    new_page->data = json_allocator_alloc(json_allocator_userdata, JSON_ARRAY_PAGE_CAP * sizeof(Json) );
    if(!new_page->data) {
      return false;
    }

    page->next = new_page;
    array->current= new_page;
    page = new_page;
  }

  void *ptr = (unsigned char *) page->data + sizeof(Json) * page->len++;
  memcpy(ptr, json, sizeof(Json));
  array->len++;
  
  return true;
}

JSON_DEF void json_array_free(Json_Array *array) {

  Json_Array_Page *page = array->first;
  while(page) {
    Json_Array_Page *next_page = page->next;
    json_allocator_free(json_allocator_userdata, page->data);
    json_allocator_free(json_allocator_userdata, page);
    page = next_page;
  }
  
  json_allocator_free(json_allocator_userdata, array);
}

// 10

//  9 / 4 = 2
//  9 % 4 = 2

//    indices  -->
//   
//   [0] [1] [2] [3]
//    0   1   2   3  [0]
//    4   5   6   7  [1]    slots
//    8   9  10  11  [2]      |
//   12  13  14  15  [3]      v
//   13  14  15  16  [4]

JSON_DEF Json json_array_get(Json_Array *array, uint64_t pos) {
  uint64_t slot = pos / JSON_ARRAY_PAGE_CAP;
  uint64_t index = pos % JSON_ARRAY_PAGE_CAP;

  Json_Array_Page *page = array->first;
  for(uint64_t i=0;i<slot;i++) page = page->next;

  return *(Json *) ( ((unsigned char *) page->data) + sizeof(Json) * index);
}

JSON_DEF bool json_object_fprint(char *key, size_t key_len, Json *json, size_t index, void *_userdata) {
  Json_Object_Data *data = (Json_Object_Data *) _userdata;

  fprintf(data->f, "\"%.*s\": ", (int) key_len, key);
  json_fprint(data->f, *json);
  if(index != data->count - 1) fprintf(data->f, ", ");
  
  return true;
}

JSON_DEF void json_fprint(FILE *f, Json json) {
  switch(json.kind) {
  case JSON_KIND_NULL: {
    fprintf(f,"null");
  } break;
  case JSON_KIND_FALSE: {
    fprintf(f,"false");
  } break;
  case JSON_KIND_TRUE: {
    fprintf(f,"true");
  } break;
  case JSON_KIND_NUMBER : {
    fprintf(f,"%2f", json.as.doubleval);
  } break;
  case JSON_KIND_STRING: {
    fprintf(f,"\"%s\"", json.as.stringval);
  } break;
  case JSON_KIND_ARRAY: {
    fprintf(f,"[");
    Json_Array *array = json.as.arrayval;
    for(size_t i=0;i<array->len;i++) {
      Json elem = json_array_get(array, i);
      json_fprint(f, elem);
      if(i != array->len - 1) fprintf(f,", ");
    }    
    fprintf(f,"]");
  } break;
  case JSON_KIND_OBJECT: {
    Json_Object_Data data = { ((Json_Hashtable *) json.as.objectval)->count, f};
    fprintf(f, "{");
    json_hashtable_for_each((Json_Hashtable *) json.as.objectval, json_object_fprint, &data);
    fprintf(f, "}");
  } break;
  default: {
    fprintf(stderr, "ERROR: Unknown json kind: %s\n", json_kind_name(json.kind) );
    exit(1);
  }
  }
}

JSON_DEF bool json_free_object(char *key, size_t key_len, Json *json, size_t index, void *_userdata) {
  (void) key;
  (void) key_len;
  (void) index;
  (void) _userdata;
  json_free(*json);
  return true;
}

JSON_DEF void json_free(Json json) {
  switch(json.kind) {
  case JSON_KIND_NULL:
  case JSON_KIND_FALSE:
  case JSON_KIND_TRUE:
  case JSON_KIND_NUMBER :
    break;
  case JSON_KIND_STRING: {
    json_allocator_free(json_allocator_userdata, (char *) json.as.stringval);
  } break;
  case JSON_KIND_ARRAY: {

    for(int i=(int) json.as.arrayval->len-1;i>=0;i--) {
      json_free(json_array_get(json.as.arrayval, i));
    }
    
    json_array_free(json.as.arrayval);
  } break;
  case JSON_KIND_OBJECT: {

    json_hashtable_for_each((Json_Hashtable *) json.as.objectval,
			    json_free_object, NULL);

    json_object_free(json.as.objectval);
  } break;
  default: {
    fprintf(stderr, "ERROR: Unknown json kind: %s\n", json_kind_name(json.kind) );
    exit(1);
  }
  }
  
}

///////////////////////////////////////////////////////////////////////////////////////

static inline uint32_t json_meiyan(const char *key, int count);

JSON_DEF bool json_hashtable_key_init(Json_Hashtable_Key **_k, const char *key, size_t key_len) {

  Json_Hashtable_Key *k = (Json_Hashtable_Key *) json_allocator_alloc(json_allocator_userdata, sizeof(Json_Hashtable_Key));
  if(!k) {
    return false;
  }
  k->len = key_len;
  k->key = (char *) json_allocator_alloc(json_allocator_userdata, sizeof(char) * k->len);
  if(!k->key) {
    return false;
  }
  memcpy(k->key, key, key_len);
  k->next = NULL;
  //k->value = -1; // TODO: check if it can be left empty

  *_k = k;
  
  return true;
}

JSON_DEF void json_hashtable_key_free(Json_Hashtable_Key *k) {
  json_allocator_free(json_allocator_userdata, k->key);
  if(k->next) json_hashtable_key_free(k->next);
  json_allocator_free(json_allocator_userdata, k);
}

static inline uint32_t json_meiyan(const char *key, int count) {
	typedef uint32_t* P;
	uint32_t h = 0x811c9dc5;
	while (count >= 8) {
		h = (h ^ ((((*(P)key) << 5) | ((*(P)key) >> 27)) ^ *(P)(key + 4))) * 0xad3e7;
		count -= 8;
		key += 8;
	}
#define tmp h = (h ^ *(uint16_t*)key) * 0xad3e7; key += 2;
	if (count & 4) { tmp tmp }
	if (count & 2) { tmp }
	if (count & 1) { h = (h ^ *key) * 0xad3e7; }
	#undef tmp
	return h ^ (h >> 16);
}

JSON_DEF bool json_hashtable_init(Json_Hashtable *ht, size_t initial_len) {

  if(initial_len == 0) {
    initial_len = 64;
  }
  ht->len = initial_len;
  ht->count = 0;
  ht->table = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Hashtable_Key*) * ht->len);
  if(!ht->table) {
    return false;
  }
  memset(ht->table, 0, sizeof(Json_Hashtable_Key*) * ht->len);
  ht->growth_treshold = 2.0;
  ht->growth_factor = 10.0;
  
  return true;
}

JSON_DEF Json_Hashtable_Ret json_hashtable_add(Json_Hashtable *ht, const char *key, size_t key_len) {
  int n = json_meiyan(key, (int) key_len) % ht->len;
  if(ht->table[n] == NULL) {
    double f = (double) ht->count / (double) ht->len;
    if(f > ht->growth_treshold ) {

#if 1
      fprintf(stderr, "ERROR: For now resizing is not supported until I implement it.\n");
      exit(1);
#else
      //printf("JSON_HASHTABLE: resizing!\n"); fflush(stdout);
      if(!json_hashtable_resize(ht, (size_t) ((double) ht->len * ht->growth_factor) )) {
	return JSON_HASHTABLE_RET_ERROR;
      }
      return json_hashtable_add(ht, key, key_len);      
#endif      
    }
    if(!json_hashtable_key_init(&ht->table[n], key, key_len)) {
      return JSON_HASHTABLE_RET_ERROR;
    }
    ht->value = &ht->table[n]->value;
    ht->count++;
    return JSON_HASHTABLE_RET_SUCCESS;
  }
  Json_Hashtable_Key *k = ht->table[n];
  while(k) {
    if(k->len == key_len && (memcmp(k->key, key, key_len) == 0) ) {
      ht->value = &k->value;
      return JSON_HASHTABLE_RET_COLLISION;
    }
    k = k->next;
  }
  ht->count++;
  Json_Hashtable_Key *old = ht->table[n];
  if(!json_hashtable_key_init(&ht->table[n], key, key_len)) {
    return JSON_HASHTABLE_RET_ERROR;
  }
  ht->table[n]->next = old;
  ht->value = &ht->table[n]->value;

  //printf("JSON_HASHTABLE: collision\n");
  return JSON_HASHTABLE_RET_SUCCESS;
}

JSON_DEF bool json_hashtable_find(Json_Hashtable *ht, const char *key, size_t key_len) {
  int n = json_meiyan(key, (int) key_len) % ht->len;
#if defined(__MINGW32__) || defined(__MINGW64__)
  __builtin_prefetch(ht->table[n]);
#endif
    
#if defined(_WIN32) || defined(_WIN64)
  _mm_prefetch((char*)ht->table[n], _MM_HINT_T0);
#endif
  Json_Hashtable_Key *k = ht->table[n];
  if (!k) return false;
  while (k) {
    if (k->len == key_len && !memcmp(k->key, key, key_len)) {
      ht->value = &k->value;
      return true;
    }
    k = k->next;
  }
  return false;
}

JSON_DEF bool json_hashtable_resize(Json_Hashtable *ht, size_t new_len) {

  size_t o = ht->len;
  Json_Hashtable_Key **old = ht->table;
  ht->len = new_len;
  ht->table = json_allocator_alloc(json_allocator_userdata, sizeof(Json_Hashtable_Key*) * ht->len);
  if(!ht->table) {
    return false;
  }
  memset(ht->table, 0, sizeof(Json_Hashtable_Key*) * ht->len);
  for(size_t i=0;i<o;i++) {
    Json_Hashtable_Key *k = old[i];
    while(k) {
      Json_Hashtable_Key *next = k->next;
      k->next = NULL;
      json_hashtable_reinsert_when_resizing(ht, k);
      k = next;
    }
  }
  json_allocator_free(json_allocator_userdata, old);
  
  return true;
}

JSON_DEF void json_hashtable_reinsert_when_resizing(Json_Hashtable *ht, Json_Hashtable_Key *k2) {
  int n = json_meiyan(k2->key, (int) k2->len) % ht->len;
  if (ht->table[n] == NULL) {
    ht->table[n] = k2;
    ht->value = &ht->table[n]->value;
    return;
  }
  Json_Hashtable_Key *k = ht->table[n];
  k2->next = k;
  ht->table[n] = k2;
  ht->value = &k2->value;
}

JSON_DEF void json_hashtable_for_each(Json_Hashtable *ht, Json_Hashtable_Func func, void *userdata) {
  size_t j = 0;
  for(size_t i=0;i<ht->len;i++) {
    if(ht->table[i] != NULL) {
      Json_Hashtable_Key *k = ht->table[i];
      while(k) {
	if(!func(k->key, k->len, &k->value, j++, userdata)) {
	  return;
	}
	k = k->next;
      }
    }
  }
}

JSON_DEF void json_hashtable_free(Json_Hashtable *ht) {
  for(size_t i=0;i<ht->len;i++) {
    if(ht->table[i]) {
      json_hashtable_key_free(ht->table[i]);
    }
  }
  json_allocator_free(json_allocator_userdata, ht->table);
}

#endif //JSON_IMPLEMENTATION

#endif //JSON_H
