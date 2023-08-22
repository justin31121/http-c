#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef HTML_PARSER_DEF
#define HTML_PARSER_DEF static inline
#endif //HTML_PARSER_DEF

#ifdef HTML_PARSER_VERBOSE
#  include <stdio.h>
#  define HTML_PARSER_LOG(...) do{ fflush(stdout); fprintf(stderr, "HTML_PARSER: " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while(0)
#else
#  define HTML_PARSER_LOG(...)
#endif //HTML_PARSER_VERBOSE

typedef bool (*Html_Parser_On_Node)(const char *name, void *arg, void **node);
typedef bool (*Html_Parser_On_Node_Attribute)(void *node, const char *key, const char *value, void *arg);
typedef bool (*Html_Parser_On_Node_Child)(void *node, void *child, void *arg);
typedef bool (*Html_Parser_On_Node_Content)(void *node, const char *content, void *arg);

typedef enum{
  HTML_PARSER_RET_ABORT = 0,
  HTML_PARSER_RET_CONTINUE = 1,
  HTML_PARSER_RET_SUCCESS = 2,
}Html_Parser_Ret;

typedef enum{
  //Expect: '<'
  HTML_PARSER_STATE_IDLE = 0,

  //Expect: '<a'
  HTML_PARSER_STATE_OPEN_BRACKET,

  //Expect: '<aaaaaaaaaaa...' | '<a>'
  HTML_PARSER_STATE_TAG_NAME,  
  
  //Exepct: TODO
  HTML_PARSER_STATE_CONTENT,
  
  //Expect 'aaa' from '</aaa>'
  HTML_PARSER_STATE_TAG_NAME_CLOSING,

  //Expect lang=\"de\"
  HTML_PARSER_STATE_ATTRIBUTES,
  HTML_PARSER_STATE_ATTRIBUTE_KEY,
  HTML_PARSER_STATE_ATTRIBUTE_QOUTE,
  HTML_PARSER_STATE_ATTRIBUTE_VALUE,

  HTML_PARSER_STATE_SINGLETON_CLOSE,

  //Expect '>' from '</a>'
  HTML_PARSER_STATE_CLOSE_BRACKET_CLOSING,
}Html_Parser_State;

#ifndef HTML_PARSER_STACK_CAP
#  define HTML_PARSER_STACK_CAP 1024
#endif //HTML_PARSER_STACK_CAP

#ifndef HTML_PARSER_BUFFER_CAP
#  define HTML_PARSER_BUFFER_CAP 1024
#endif //HTML_PARSER_BUFFER_CAP

#define HTML_PARSER_BUFFER 0
#define HTML_PARSER_KEY 1
#define HTML_PARSER_VALUE 2

typedef struct{
  char buffer[3][HTML_PARSER_BUFFER_CAP];
  size_t buffer_size[3];

  const char *stack[HTML_PARSER_STACK_CAP];
  size_t stack_size;
  
  char *name;
  size_t name_len;
  size_t name_i;

  bool in_script;
  size_t depth;
  
  Html_Parser_State state;
  Html_Parser_State prev;
}Html_Parser;

HTML_PARSER_DEF Html_Parser html_parser(void);
HTML_PARSER_DEF Html_Parser_Ret html_parser_consume(Html_Parser *parser, const char *data, size_t size);

#ifdef HTML_PARSER_IMPLEMENTATION


#define html_parser_print_key_value(parser) do{			\
    for(size_t i=0;i<parser->depth+1;i++) printf("\t");		\
    printf("'%.*s'='%.*s'\n",					\
	   (int) parser->buffer_size[HTML_PARSER_KEY],		\
	   parser->buffer[HTML_PARSER_KEY],			\
	   (int) parser->buffer_size[HTML_PARSER_VALUE],	\
	   parser->buffer[HTML_PARSER_VALUE]);			\
  }while(0)

#define html_parser_buffer_equals(buf, size, cstr) ((int) strlen((cstr)) == (size) && (strncmp((buf), (cstr), (size)) == 0))
#define html_parser_append(parser, c) do{				\
    if(parser->buffer_size[HTML_PARSER_BUFFER] >= HTML_PARSER_BUFFER_CAP) { \
      parser->buffer_size[HTML_PARSER_BUFFER] = 0;			\
    }									\
    parser->buffer[HTML_PARSER_BUFFER][parser->buffer_size[HTML_PARSER_BUFFER]++] = c; \
  }while(0)

HTML_PARSER_DEF Html_Parser html_parser(void) {
  Html_Parser parser = {0};
  parser.state = HTML_PARSER_STATE_IDLE;
  parser.stack_size = 0;
  parser.depth = 0;
  parser.in_script = false;
  return parser;
}

HTML_PARSER_DEF Html_Parser_Ret html_parser_consume(Html_Parser *parser, const char *data, size_t size) {

 consume:
  switch(parser->state) {
  case HTML_PARSER_STATE_IDLE: {
    idle:
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(isspace(data[0])) {
      data++;
      size--;
      if(size) goto idle;
    }

    if(data[0] == '<') {
      parser->state = HTML_PARSER_STATE_OPEN_BRACKET;
      data++;
      size--;
      if(size) goto consume;
    } else {
      HTML_PARSER_LOG("Expected '<' but found: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;
    }
    
    return HTML_PARSER_RET_CONTINUE;
  } break;
  case HTML_PARSER_STATE_OPEN_BRACKET: {
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(data[0] == '/') {

      parser->state = HTML_PARSER_STATE_TAG_NAME_CLOSING;
      assert(parser->stack_size);
      parser->name = (char *) parser->stack[--parser->stack_size];
      parser->name_len = strlen(parser->name);
      parser->name_i = 0;

      data++;
      size--;
      if(size) goto consume;

      return HTML_PARSER_RET_CONTINUE;
    } else if(isalpha(data[0])) {
      parser->buffer_size[HTML_PARSER_BUFFER] = 0;
      parser->prev = parser->state;
      parser->state = HTML_PARSER_STATE_TAG_NAME;      
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else {

      if(parser->in_script) {
	html_parser_append(parser, '<');
	html_parser_append(parser, data[0]);

	data++;
	size--;
	parser->state = HTML_PARSER_STATE_CONTENT;
	if(size) goto consume;
	return HTML_PARSER_RET_CONTINUE;
      } else {	
      
	HTML_PARSER_LOG("Expecting alphabetic char but got: '%c'", data[0]);
	return HTML_PARSER_RET_ABORT;
      }
    }

  } break;
  case HTML_PARSER_STATE_ATTRIBUTES: {
    attribute:

    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(isspace(data[0])) {
      data++;
      size--;
      goto attribute;
    } else if(data[0] == '/') {

      parser->state = HTML_PARSER_STATE_SINGLETON_CLOSE;
      
      data++;
      size--;
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else if(data[0] == '>') {      
      
      parser->prev  = parser->state;
      parser->state = HTML_PARSER_STATE_TAG_NAME;
      
      goto consume;
    } else if(isalpha(data[0])) {
      
      parser->state = HTML_PARSER_STATE_ATTRIBUTE_KEY;
      parser->buffer_size[HTML_PARSER_KEY] = 0;
      
      goto consume;
    } else {
      HTML_PARSER_LOG("Expected attribute or '>' but got: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;
    }
    
    return HTML_PARSER_RET_ABORT;
  } break;
  case HTML_PARSER_STATE_ATTRIBUTE_KEY: {
    attribute_key:
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(isalpha(data[0]) || data[0] == '-') {
      assert(parser->buffer_size[HTML_PARSER_KEY] < HTML_PARSER_BUFFER_CAP);
      parser->buffer[HTML_PARSER_KEY][parser->buffer_size[HTML_PARSER_KEY]++] = data[0];
      data++;
      size--;
      goto attribute_key;
    } else if(data[0] == '='){
      parser->state = HTML_PARSER_STATE_ATTRIBUTE_QOUTE;
      parser->buffer_size[HTML_PARSER_VALUE] = 0;
      data++;
      size--;      
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else if(data[0] == '>') {

      html_parser_print_key_value(parser);
      
      parser->prev  = parser->state;
      parser->state = HTML_PARSER_STATE_TAG_NAME;
      goto consume;
    } else if(isspace(data[0])) {
      
      html_parser_print_key_value(parser);
      
      parser->state = HTML_PARSER_STATE_ATTRIBUTES;
      goto consume;
    } else {
      HTML_PARSER_LOG("Expected attribute-key or '\"' but got: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;
    }
  } break;
  case HTML_PARSER_STATE_ATTRIBUTE_QOUTE: {
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(data[0] == '\"') {
      data++;
      size--;
      parser->state = HTML_PARSER_STATE_ATTRIBUTE_VALUE;
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else {
      HTML_PARSER_LOG("Expected '\"' but got: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;
    }
  } break;
  case HTML_PARSER_STATE_ATTRIBUTE_VALUE: {
    attribute_value:

    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(data[0] == '\"') {

      html_parser_print_key_value(parser);
      parser->buffer_size[HTML_PARSER_VALUE] = 0;
      
      parser->state = HTML_PARSER_STATE_ATTRIBUTES;
      data++;
      size--;
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else {
      assert(parser->buffer_size[HTML_PARSER_VALUE] < HTML_PARSER_BUFFER_CAP);
      parser->buffer[HTML_PARSER_VALUE][parser->buffer_size[HTML_PARSER_VALUE]++] = data[0];

      data++;
      size--;
      goto attribute_value;
    }
    
  } break;
  case HTML_PARSER_STATE_TAG_NAME: {
    tag_name:
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(isalpha(data[0]) || data[0] == '-') {
      html_parser_append(parser, data[0]);

      data++;
      size--;
      if(size) goto tag_name;
    } else if(data[0] == '>') {

      if(parser->in_script) {
	//TODO

	parser->state = HTML_PARSER_STATE_CONTENT;
      } else {

	char *buf = parser->buffer[HTML_PARSER_BUFFER];
	size_t buf_size = parser->buffer_size[HTML_PARSER_BUFFER];
	if( html_parser_buffer_equals(buf, buf_size, "link") ||
	    html_parser_buffer_equals(buf, buf_size, "meta") ||
	    html_parser_buffer_equals(buf, buf_size, "input")) {

	  for(size_t i=0;i<parser->depth;i++) printf("\t");
	  printf("NODE Html-Singleton: '%.*s'\n", (int) buf_size, buf);
	} else {
	  assert(buf_size);
	  char *name = malloc((buf_size + 1) * sizeof(char));
	  if(!name) {
 	    HTML_PARSER_LOG("Failed to allocate memory");
	    return HTML_PARSER_RET_ABORT;
	  }
	  memcpy(name, buf, buf_size);
	  name[buf_size] = 0;
	  assert(parser->stack_size < HTML_PARSER_STACK_CAP);
	  parser->stack[parser->stack_size++] = name;

	  if(strcmp(name, "script") == 0) {
	    parser->in_script = true;
	  } else {
	    parser->in_script = false;
	  }

	  for(size_t i=0;i<parser->depth;i++) printf("\t");
	  printf("NODE Open: '%.*s'\n", (int) buf_size, buf);
	  parser->depth++;
	}
	
	parser->state = HTML_PARSER_STATE_CONTENT;
	parser->buffer_size[HTML_PARSER_BUFFER] = 0;

      }
      
      data++;
      size--;
      if(size) goto consume;
    } else if(isspace(data[0])) {
      parser->state = HTML_PARSER_STATE_ATTRIBUTES;
      goto consume;
    } else {

      if(parser->in_script) {
	html_parser_append(parser, '<');
	html_parser_append(parser, data[0]);
	
	data++;
	size--;
	parser->state = HTML_PARSER_STATE_CONTENT;
	if(size) goto consume;
      } else {	
	HTML_PARSER_LOG("Expected tag-name or '>' but found: '%c'", data[0]);
	return HTML_PARSER_RET_ABORT;
      }
      
    }    

    return HTML_PARSER_RET_CONTINUE;
  } break;
  case HTML_PARSER_STATE_CONTENT: {
    content:
    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(data[0] == '<') {
      parser->state = HTML_PARSER_STATE_OPEN_BRACKET;
      data++;
      size--;

      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else {
      html_parser_append(parser, data[0]);
      
      data++;
      size--;

      if(size) goto content;
      return HTML_PARSER_RET_CONTINUE;
    }
  } break;
  case HTML_PARSER_STATE_TAG_NAME_CLOSING: {
    tag_name_closing:

    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(parser->name_i >= parser->name_len) {
      parser->state = HTML_PARSER_STATE_CLOSE_BRACKET_CLOSING;
      if(parser->in_script && strcmp(parser->name, "script") == 0) {
	parser->in_script = false;
      }
      parser->depth--;
      for(size_t i=0;i<parser->depth;i++) printf("\t");
      printf("NODE Close: '%.*s'\n", (int) parser->name_len, parser->name);
      free(parser->name);
      parser->name = NULL;
      goto consume;
    }

    if(data[0] == parser->name[parser->name_i]) {
      parser->name_i++;
      data++;
      size--;
      if(size) goto tag_name_closing;
      return HTML_PARSER_RET_CONTINUE;
    } else {
      HTML_PARSER_LOG("Expected '%c' from '%s'(index: %zu) but found '%c'", parser->name[parser->name_i], parser->name, parser->name_i, data[0]);
      return HTML_PARSER_RET_ABORT;
    }
  } break;
  case HTML_PARSER_STATE_CLOSE_BRACKET_CLOSING: {

    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(data[0] == '>') {
      if(parser->stack_size == 0) {
	return HTML_PARSER_RET_SUCCESS;
      }
      
      parser->state = HTML_PARSER_STATE_CONTENT;
      parser->buffer_size[HTML_PARSER_BUFFER] = 0;
      data++;
      size--;
      if(size) goto consume;

      return HTML_PARSER_RET_CONTINUE;
    } else {
      HTML_PARSER_LOG("Expected '>' but found: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;

    }
  } break;
  case HTML_PARSER_STATE_SINGLETON_CLOSE: {
    singleton_close:

    if(!size)
      return HTML_PARSER_RET_CONTINUE;

    if(isspace(data[0])) {
      data++;
      size--;
      goto singleton_close;
    } else if(data[0] == '>') {

      char *buf = parser->buffer[HTML_PARSER_BUFFER];
      size_t buf_size = parser->buffer_size[HTML_PARSER_BUFFER];

      for(size_t i=0;i<parser->depth;i++) printf("\t");
      printf("NODE Singleton: '%.*s'\n", (int) buf_size, buf);
      
      parser->state = HTML_PARSER_STATE_CONTENT;
      parser->buffer_size[HTML_PARSER_BUFFER] = 0;
      data++;
      size--;
      if(size) goto consume;
      return HTML_PARSER_RET_CONTINUE;
    } else {
      HTML_PARSER_LOG("Expected '>' but found: '%c'", data[0]);
      return HTML_PARSER_RET_ABORT;
    }
    
  } break;
  default: {
    HTML_PARSER_LOG("unknown state in html_parser_consume");
    return HTML_PARSER_RET_ABORT;
  } break;
  }

  HTML_PARSER_LOG("unknown code in html_parser_consume");
  return HTML_PARSER_RET_ABORT;
}

#endif //HTML_PARSER_IMPLEMENATATION

#endif //HTML_PARSER_H
