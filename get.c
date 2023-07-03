
#define REGION_IMPLEMENTATION
#include "region.h"

#define HTTP_IMPLEMENTATION
#define HTTP_SSL
#include "http.h"

#include "common.h"

static size_t count  = 0;

Http_Ret region_callback(const char *data, size_t size, Region_Linear *region) {

  Region_Ptr ptr;
  if(!region_linear_alloc(&ptr, region, size)) {
    fprintf(stderr, "ERROR allocaint memory in region\n");
    return false;
  }
  memcpy( region_deref_linear(ptr), data, size);
  count += size;
  
  return HTTP_RET_CONTINUE;
}

int main() {

  Region_Linear region;
  if(!region_linear_init(&region, 4096))
    panic("region_init");

  const char *hostname = "www.youtube.com";

  Http http;
  if(!http_init(&http, hostname, HTTPS_PORT, true))
    panic("http_init");

  if(!http_request(&http, "/", "GET", NULL, 0, region_callback, &region, "Connection: Close\r\n"))
    panic("http_request");

  http_free(&http);

  printf("count: %lld\n", count);
  printf("%.*s", (int) count, (char *) region_base(region) );

  return 0;
}
























/*

    char spotify_creds[1024];
  if(!io_getenv("SPOTIFY_CREDS", spotify_creds, sizeof(spotify_creds))) 
    panic("The envorinment variable 'SPOTIFY_CREDS' is not set");


  
SPOTIFY_DEF bool spotify_get_access_token(char *spotify_creds, String_Buffer *sb, string *access_token) {
    size_t sb_len = sb->len;
    string key = tsmap(sb, string_from_cstr(spotify_creds), base64_encode);
    const char *auth = tprintf(sb, "Authorization: Basic "String_Fmt"\r\n", String_Arg( key ));

    sb->len = sb_len;
    if(!http_post("https://accounts.spotify.com/api/token",
		  "grant_type=client_credentials",
		  "application/x-www-form-urlencoded",
		  string_buffer_callback, sb, NULL, auth)) {
	return false;
    }
    string response = string_from(sb->data + sb_len, sb->len - sb_len);

    Spotify_Auth spotify_auth = {0};
    Json_Parse_Events events = {0};
    events.on_elem = spotify_on_elem_access_token;
    events.on_object_elem = spotify_on_object_elem_access_token;
    events.arg = &spotify_auth;
    if(!json_parse2(response.data, response.len, &events)) {
	return false;
    }
  
    if(!spotify_auth.token.len) {
	return false;
    }
    sb->len = sb_len;
    string_buffer_append(sb, spotify_auth.token.data, spotify_auth.token.len);

    *access_token = string_from(sb->data + sb->len - spotify_auth.token.len, spotify_auth.token.len);
  
    return true;    
}
*/

