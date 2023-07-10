#define REGION_IMPLEMENTATION
#define REGION_LINEAR
#define REGION_DYNAMIC
#include "../src/region.h"

#define JSON_IMPLEMENTATION
#include "../src/json.h"

#define JSON_PARSER_IMPLEMENTATION
#include "../src/json_parser.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "../src/http_parser.h"

#define HTTP_IMPLEMENTATION
#define HTTP_WIN32_SSL
#include "../src/http.h"


#define IO_IMPLEMENTATION
#include "../src/io.h"

#define BASE64_IMPLEMENTATION
#include "../src/base64.h"

#define URL_IMPLEMENTATION
#include "../src/url.h"

#include "../src/common.h"

Http_Parser_Ret region_callback(Region *region, const char *data, size_t size) {
  Region_Ptr ptr;
  if(!region_alloc(&ptr, region, size)) return HTTP_PARSER_RET_ABORT;  
  memcpy( region_deref(ptr), data, size);
  return HTTP_PARSER_RET_CONTINUE;
}

typedef struct{
    Json *json;
    bool got_root;
}Json_Ctx;

bool on_elem_json(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {

    Json_Ctx *ctx = (Json_Ctx *) arg;
  
    Json *json = malloc(sizeof(Json));
    if(!json) return false;
  
    switch(type) {
    case JSON_PARSER_TYPE_OBJECT: {
	json->kind = JSON_KIND_OBJECT;
	if(!json_object_init(&json->as.objectval)) return false;    
    } break;
    case JSON_PARSER_TYPE_STRING: {
	json->kind = JSON_KIND_STRING;
	if(!json_string_init2((char **) &json->as.stringval, content, content_size)) return false;
    } break;
    case JSON_PARSER_TYPE_NUMBER: {
	double num = strtod(content, NULL);
	*json = json_number(num);
    } break;
    case JSON_PARSER_TYPE_ARRAY: {
	json->kind = JSON_KIND_ARRAY;
	if(!json_array_init(&json->as.arrayval)) return false;        
    } break;
    case JSON_PARSER_TYPE_FALSE: {
	*json = json_false();
    } break;
    case JSON_PARSER_TYPE_TRUE: {
	*json = json_true();
    } break;
    case JSON_PARSER_TYPE_NULL: {
	*json = json_null();
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

    *elem = json;

    return true;
}

bool on_object_elem_json(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
    (void) arg;
    //Json_Ctx *ctx = (Json_Ctx *) arg;

    Json *json = (Json *) object;
    Json *smol = (Json *) elem;
  
    if(!json_object_append2(json->as.objectval, key_data, key_size, smol)) {
	return false;
    }

    return true;
}

bool on_array_elem_json(void *array, void *elem, void *arg) {
    (void) arg;

    Json *json = (Json *) array;
    Json *smol = (Json *) elem;
  
    if(!json_array_append(json->as.arrayval, smol)) {
	return false;
    }

    return true;
}

typedef struct{
  const char *prev;
  size_t prev_size;

  Region *region;
  string out;
}Access_Token;

bool on_elem_access_token(Json_Parser_Type type, const char *content, size_t content_size, void *arg, void **elem) {
  (void) type;
  (void) elem;

  Access_Token *a = (Access_Token *) arg;
  
  a->prev = content;
  a->prev_size = content_size;

  return true;
}

bool on_object_elem_access_token(void *object, const char *key_data, size_t key_size, void *elem, void *arg) {
  (void) elem;
  (void) object;

  Access_Token *a = (Access_Token *) arg;

  if(strncmp("access_token", key_data, key_size) == 0) {
    if(!string_alloc2(&a->out, a->region, a->prev, a->prev_size)) return false;
  }

  return true;
}

bool get_access_token(const char *spotify_creds, Region *temp, string *access_token) {
  Region_Ptr current = region_current(*temp);
  
  string creds;
  if(!string_alloc(&creds, temp, spotify_creds))
    return false;

  string creds_base64;
  if(!string_map(&creds_base64, temp, creds, base64_encode))
    return false;
  
  string headers;
  if(!string_snprintf(&headers, temp,
		      "Authorization: Basic "str_fmt"\r\n"
		      "Content-Type: application/x-www-form-urlencoded\r\n"
		      "\0", str_arg(creds_base64) ))
    return false;
  
  ////////////////////////////////////////

  Http http;
  if(!http_init(&http, "accounts.spotify.com", HTTPS_PORT, true))
    return false;

  Access_Token ctx;
  ctx.prev = NULL;
  ctx.prev_size = 0;
  ctx.region = temp;
  ctx.out = (string) {0};

  Json_Parser jparser = json_parser();
  jparser.on_elem = on_elem_access_token;
  jparser.on_object_elem = on_object_elem_access_token;
  jparser.arg = &ctx;
  Http_Parser parser =
    http_parser((Http_Parser_Write_Callback) json_parser_consume, NULL, &jparser);

  region_rewind(temp, current);
  const char *body = "grant_type=client_credentials";
  if(http_request(&http, "/api/token", "POST",
		  (const unsigned char *) body, (int) strlen(body),
		  (Http_Write_Callback) http_parser_consume, &parser,
		  (const char *) region_deref(headers.data) ) != HTTP_RET_SUCCESS) {
    return false;
  }

  http_free(&http);

  region_rewind(temp, current);
  string_copy(access_token, temp, ctx.out);
    
  return true;
}

void dump_results(Json json) {
    Json albums = json_null();
    Json tracks = json_null();
    Json playlists = json_null();

    if( json_object_has(json.as.objectval, "albums") &&
	( (albums = json_object_get(json.as.objectval, "albums")),
	  json_object_has(albums.as.objectval, "items"))  ) {
	albums = json_object_get(albums.as.objectval, "items");
    }

    if( json_object_has(json.as.objectval, "tracks") &&
	( (tracks = json_object_get(json.as.objectval, "tracks")),
	  json_object_has(tracks.as.objectval, "items"))  ) {
	tracks = json_object_get(tracks.as.objectval, "items");
    }

    if( json_object_has(json.as.objectval, "playlists") &&
	( (playlists = json_object_get(json.as.objectval, "playlists")),
	  json_object_has(playlists.as.objectval, "items"))  ) {
	playlists = json_object_get(playlists.as.objectval, "items");
    }

 
    if( albums.kind != JSON_KIND_NULL ) {
	size_t len = json_array_len(albums.as.arrayval);

	for(size_t i=0;i<len;i++) {
	    Json album = json_array_get(albums.as.arrayval, i);

	    const char *name = json_object_get(album.as.objectval, "name").as.stringval;
	    const char *id = json_object_get(album.as.objectval, "id").as.stringval;
	    double track_count = json_object_get(album.as.objectval, "total_tracks").as.doubleval;

	    Json artists = json_object_get(album.as.objectval, "artists");
	    size_t artists_len = json_array_len(artists.as.arrayval);
	  
	    printf("[%2zu] Album  : %s \n", i, name);
	    printf("     Artists: ");
	    for(size_t j=0;j<artists_len;j++) {
		Json artist = json_array_get(artists.as.arrayval, j);
		const char *artist_name = json_object_get(artist.as.objectval, "name").as.stringval;
		printf("%s", artist_name);
		if(j != artists_len - 1) printf(", ");
	    }
	    printf("\n");
	    printf("     Url    : https://open.spotify.com/album/%s\n", id);
	    printf("     Tracks : %d\n", (int) track_count);
	    printf("\n");
	}
    }

    printf("============================================================================================\n");

    if( tracks.kind != JSON_KIND_NULL ) {
	size_t len = json_array_len(tracks.as.arrayval);

	for(size_t i=0;i<len;i++) {
	    Json track = json_array_get(tracks.as.arrayval, i);

	    const char *name = json_object_get(track.as.objectval, "name").as.stringval;
	    const char *id = json_object_get(track.as.objectval, "id").as.stringval;
	    const char *album =
		json_object_get(json_object_get(track.as.objectval, "album").as.objectval, "name").as.stringval;

	    Json artists = json_object_get(track.as.objectval, "artists");
	    size_t artists_len = json_array_len(artists.as.arrayval);
	  
	    printf("[%2zu] Track    : %s \n", i, name);
	    printf("     Album    : %s\n", album);
	    printf("     Artists  : ");
	    for(size_t j=0;j<artists_len;j++) {
		Json artist = json_array_get(artists.as.arrayval, j);
		const char *artist_name = json_object_get(artist.as.objectval, "name").as.stringval;
		printf("%s", artist_name);
		if(j != artists_len - 1) printf(", ");
	    }
	    printf("\n");
	    printf("     Url      : https://open.spotify.com/track/%s\n", id);

	    printf("\n");
	}
    }

    printf("============================================================================================\n");

    if( playlists.kind != JSON_KIND_NULL ) {
	size_t len = json_array_len(playlists.as.arrayval);

	for(size_t i=0;i<len;i++) {
	    Json playlist = json_array_get(playlists.as.arrayval, i);

	    const char *name = json_object_get(playlist.as.objectval, "name").as.stringval;
	    const char *id = json_object_get(playlist.as.objectval, "id").as.stringval;
	  
	    printf("[%2zu] Playlist    : %s \n", i, name);
	    printf("     Url         : https://open.spotify.com/playlist/%s\n", id);

	    printf("\n");
	}
    }
   
}

bool search_keyword(const char *keyword, string access_token, Region *temp, Json *out) {

    Region_Ptr current = region_current(*temp);

    string input_encoded;
    if(!string_map_cstr(&input_encoded, temp, keyword, url_encode))
	return false;

    string url;
    if(!string_snprintf(&url, temp,
			"/v1/search?q="str_fmt
			"&type=track,playlist,album"
			"\0", str_arg(input_encoded) ))
	return false;

    string auth;
    if(!string_snprintf(&auth, temp,
			"Authorization: Bearer "str_fmt"\r\n"
			"\0", str_arg(access_token) ))
	return false;

    region_rewind(temp, current);

    Http http;
    if(!http_init(&http, "api.spotify.com", HTTPS_PORT, true))
	return false;

    Json_Ctx ctx = { NULL, false };

    Json_Parser jparser = json_parser();
    jparser.on_elem = on_elem_json;
    jparser.on_object_elem = on_object_elem_json;
    jparser.on_array_elem = on_array_elem_json;
    jparser.arg = &ctx;
  
    Http_Parser parser =
	http_parser( (Http_Parser_Write_Callback) json_parser_consume, NULL, &jparser );
  
    if(http_request(&http, (const char *) region_deref(url.data), "GET",
		    NULL, 0,
		    (Http_Write_Callback) http_parser_consume, &parser,
		    (const char *) region_deref(auth.data) ) != HTTP_RET_SUCCESS )
	return false;

    http_free(&http);
  
    if(parser.response_code != 200 || !ctx.got_root) {
	fprintf(stderr,
		"ERROR: Something failed. Either parsing the request or\n"
		"       the request itself\n");
	return false;
    }

    *out = *ctx.json;
    
    return true;
}

int main(int argc, char **argv) {

  const char *program = argv[0];
  if(argc < 2) {
    fprintf(stderr, "ERROR: Please provide an argument\n");
    fprintf(stderr, "USAGE: %s <search-term>\n", program);
    return 1;
  }
  const char *input = argv[1];

  Region region;
  if(!region_init(&region, 1))
    panic("region_init");

  char spotify_creds[1024];
  if(!io_getenv("SPOTIFY_CREDS", spotify_creds, sizeof(spotify_creds)))  {
      fprintf(stderr,
	      "ERROR:\n"
	      "      The environment variable: 'SPOTIFY_CREDS' is not set.\n"
	      "      In order to run this demo, you have to request an api-access\n"
	      "      from spotify at: \n"
	      "\n"
	      "          'https://developer.spotify.com/documentation/web-api'.\n"
	      "\n"
	      "      After that you will receive a client-id and client-secret.\n"
	      "      Finally you have to set the credentiels as the environment variable:\n"
	      "      'SPOTIFY_CREDS' or provide it somehow different into the demo, as\n"
	      "      a NULL-terminated-string.\n"
	      "\n"
	      "      It should look like this:\n"
	      "\n"
	      "          'abcdefghijklmnopqrstufvwxyz12345:67890abcdefghijklmnopqrstuvwxyz1'\n"
	      "\n"
	      "      where the scheme is:\n"
	      "\n"
	      "          ${client-id}:${client-secret}\n");
      return 1;
  }

  string access_token;
  if(!get_access_token(spotify_creds, &region, &access_token))
    panic("get_access_token");
  
  //printf( str_fmt, str_arg(access_token) );

  //////////////////////////////////////////////////////

  Json json;
  if(!search_keyword(input, access_token, &region, &json))
      panic("search");

  dump_results(json);
  
  return 0;
}

