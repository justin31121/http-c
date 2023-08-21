#include <stdio.h>

#define HTML_PARSER_BUFFER_CAP (1024 * 2)

#define HTML_PARSER_IMPLEMENTATION
#define HTML_PARSER_VERBOSE
#include "../src/html_parser.h"

#define IO_IMPLEMENTATION
#include "../src/io.h"

#define CHECK_HTML(payload) do{						\
    Html_Parser parser = html_parser();					\
    Html_Parser_Ret ret =						\
      html_parser_consume(&parser, (payload), strlen((payload)));	\
    if(ret == HTML_PARSER_RET_ABORT) {					\
      fprintf(stderr, "\"%s\" => ABORT\n", payload);			\
      return 1;								\
    } else if(ret == HTML_PARSER_RET_CONTINUE) {				\
      fprintf(stderr, "\"%s\" => CONTINUE\n", payload);			\
      return 1;								\
    } else {								\
      printf("\"%s\" => Passed!\n", payload);				\
   }									\
  }while(0)

int main() {

  char *buffer;
  size_t buffer_size;
  if(!io_slurp_file("rsc/youtube.html", &buffer, &buffer_size)) {
    return 1;
  }

  Html_Parser parser = html_parser();

  //<!DOCTYPE html>
  buffer += 15;
  buffer_size -= 15;
  
  size_t i;
  for(i=0;i<buffer_size;i++) {
    //printf("parsing: '%c'\n", buffer[i]);
    Html_Parser_Ret ret = html_parser_consume(&parser, buffer + i, 1);

    int len = (int) (buffer_size - i);
    if(len > 100) len = 100;
    if(ret == HTML_PARSER_RET_ABORT) {
      fprintf(stderr, "ERROR: Failed to parse at [%zu]: '%.*s'\n", i, len, buffer + i);
      return 1;
    } else if(ret == HTML_PARSER_RET_CONTINUE) {
      //pass
    } else if(i != buffer_size - 1) {
      fprintf(stderr, "ERROR: Parsed to early at [%zu]: '%.*s'\n", i, len, buffer + i);
      return 1;
    } else {
      return 0;
    }
  }

  fprintf(stderr, "ERROR: Parser wants more\n");
  
  return 1;
}

int main1() {

  CHECK_HTML("<html></html>");
  CHECK_HTML("<html><head></head></html>");
  CHECK_HTML("<html>Hello, World!</html>");
  CHECK_HTML("<html>Foo<head></head>Bar</html>");
  CHECK_HTML("<html>\n"
	    "  Foo\n"
	    "  <head>\n"
	    "    <title></title>\n"
	    "  </head>\n"
	    "  Bar\n"
	    "  <body>\n"
	    "      <div></div>\n"
	    "  </body>\n"
	    "</html>");
  CHECK_HTML("<html lang=\"de\" special-value=\"value\"></html>");
  CHECK_HTML("<html><super-div></super-div></html>");
  CHECK_HTML("<html yoyo super static void public></html>");
  CHECK_HTML("<html><input type=\"text\"></input></html>");
  CHECK_HTML("<html><div><input /></div></html>");
  CHECK_HTML("<html><input /><div></div></html>");
  CHECK_HTML("<html><input /></html>");
  
  printf("\nAll okay");
  return 0;
}
