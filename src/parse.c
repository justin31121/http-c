#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

#define HTTP_IMPLEMENTATION
#include "http.h"

#include "common.h"

int main () {

    Http http;
    if(!http_init(&http, "www.example.com", HTTP_PORT, false))
	panic("http_init");

    Http_Parser parser = http_parser((Http_Parser_Write_Callback) http_fwrite, NULL, stdout);

    if(http_request(&http, "/", "GET",
		    NULL, 0,
		    (Http_Write_Callback) http_parser_consume, &parser,
		    "Connection: Close\r\n") == HTTP_RET_ABORT)
	panic("http_request");

    http_free(&http);

    printf("Response Code : %d\n", parser.response_code);
    printf("Content Length: %lld bytes\n", parser.content_length);
    
    return 0;
}
