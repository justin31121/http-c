#ifndef HTTP_H
#define HTTP_H

#ifndef HTTP_DEF
#  define HTTP_DEF static inline
#endif // HTTP_DEF

#ifndef HTTP_BUFFER_SIZE
#  define HTTP_BUFFER_SIZE 8192
#endif //HTTP_BUFFER_SIZE

#ifndef HTTP_ENTRY_SIZE
#  define HTTP_ENTRY_SIZE 2048
#endif // HTTP_ENTRY_SIZE

#ifndef HTTP_LOG
#  ifdef HTTP_QUIET
#    define HTTP_LOG(...)
#  else
#    include <stdio.h>
#    define HTTP_LOG(...) fprintf(stderr, "HTTP_LOG: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#  endif // HTTP_QUIET
#endif // HTTP_LOG

#define HTTP_PORT 80
#define HTTPS_PORT 443

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include <assert.h>

#ifdef _WIN32
#  include <ws2tcpip.h>
#  include <windows.h>
#elif linux
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

#ifdef HTTP_OPEN_SSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#endif //HTTP_OPEN_SSL

typedef struct{
#ifdef _WIN32
  SOCKET socket;
#else
  int socket;
#endif

#ifdef HTTP_OPEN_SSL
  SSL *conn;  
#endif // HTTP_OPEN_SSL
  
  const char *hostname;
}Http;

HTTP_DEF bool http_init(const char* hostname, uint16_t port, bool use_ssl, Http *http);

HTTP_DEF bool http_socket_write(const char *data, size_t size, void *http);
HTTP_DEF bool http_socket_read(char *data, size_t size, void *http, size_t *read);

HTTP_DEF bool http_socket_connect_plain(Http *http, const char *hostname, uint16_t port);
HTTP_DEF bool http_socket_write_plain(const char *data, size_t size, void *_http);
HTTP_DEF bool http_socket_read_plain(char *buffer, size_t buffer_size, void *_http, size_t *read);

HTTP_DEF void http_free(Http *http);

typedef struct{
  Http *http;

  // Buffer read's
  char buffer[HTTP_BUFFER_SIZE];
  size_t buffer_pos, buffer_size;

  // Parsing
  char key[HTTP_ENTRY_SIZE];
  size_t key_len;
  char value[HTTP_ENTRY_SIZE];
  size_t value_len;
  int body, state, state2, pair;
  size_t content_read;

  // Info
  int response_code;
  size_t content_length;
  
}Http_Request;

typedef struct{
  char *key, *value;
  size_t key_len, value_len;
}Http_Header;

HTTP_DEF bool http_request_from(Http *http, const char *route, const char *method,
				const char *headers,
				const unsigned char *body, size_t body_len,
				Http_Request *request);
HTTP_DEF bool http_next_header(Http_Request *r, Http_Header *entry);
HTTP_DEF bool http_next_body(Http_Request *r, char **data, size_t *data_len);

HTTP_DEF bool http_maybe_init_external_libs();

HTTP_DEF bool http_parse_u64(char *buffer, size_t buffer_len, uint64_t *out);
HTTP_DEF bool http_parse_hex_u64(char *buffer, size_t buffer_len, uint64_t *out);
HTTP_DEF bool http_header_eq(const char *key, size_t key_len, const char *value, size_t value_len);

//////////////////////////////////////////////////////////////////////////////////////////////

typedef bool (*Http_Sendf_Callback)(const char *data, size_t size, void *userdata);

typedef struct{
  Http_Sendf_Callback send_callback;
  char *buffer;
  size_t buffer_cap;
  void *userdata;
  bool last;
}Http_Sendf_Context;

HTTP_DEF bool http_sendf(Http_Sendf_Callback send_callback, void *userdata,
			 char *buffer, size_t buffer_cap, const char *format, ...);
HTTP_DEF bool http_sendf_impl(Http_Sendf_Callback send_callback, void *userdata,
			      char *buffer, size_t buffer_cap, const char *format, va_list args);
HTTP_DEF size_t http_sendf_impl_send(Http_Sendf_Context *context, size_t *buffer_size, const char *cstr, size_t cstr_len);
HTTP_DEF size_t http_sendf_impl_copy(Http_Sendf_Context *context, size_t buffer_size,
				     const char *cstr, size_t cstr_len, size_t *cstr_off);

#ifdef HTTP_IMPLEMENTATION

#ifdef _WIN32
#  define HTTP_LOG_OS(method) char msg[1024]; FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &msg, sizeof(msg), NULL); HTTP_LOG((method)"-error: (%d) %s", GetLastError(), msg);
#else
#  define HTTP_LOG_OS(method) HTTP_LOG((method)"-error: (%d) %s", errno, strerr(errno))
#endif // _WIN32

#ifdef _WIN32
static bool http_global_wsa_startup = false;
#endif //_WIN32

#ifdef HTTP_OPEN_SSL
static SSL_CTX *http_global_ssl_context = NULL;
#endif //HTTP_OPEN_SSL

HTTP_DEF bool http_init(const char* hostname, uint16_t port, bool use_ssl, Http *h) {

  size_t hostname_len = strlen(hostname);
  h->hostname = malloc(hostname_len + 1);
  if(!h->hostname) {
    return false;
  }
  memcpy((char *) h->hostname, hostname, hostname_len + 1);

  if(!http_maybe_init_external_libs()) {
    return false;
  }

#ifdef _WIN32
  h->socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
  if( h->socket == INVALID_SOCKET ) {
    HTTP_LOG("Failed to initialize socket");
    return false;
  }
#elif linux
  h->socket = socket(AF_INET, SOCK_STREAM, 0);
  if( h->socket == -1) {
    HTTP_LOG("Failed to initialize socket");
    return false;
  }
#else
  HTTP_LOG("Unsupported platform. Implement: http_init");
  
  return false;
#endif // _WIN32

  if(!http_socket_connect_plain(h, hostname, port)) {
    HTTP_LOG("Can not connect to '%s:%u'", hostname, port);
    return false;
  }

#ifdef HTTP_OPEN_SSL    
  h->conn = NULL;
  
  if(use_ssl) {
    h->conn = SSL_new(http_global_ssl_context);
    if(!h->conn) {
      HTTP_LOG("Fatal error using OPEN_SSL");
      return false;
    }
    SSL_set_fd(h->conn, (int) h->socket); // TODO: maybe check this cast

    SSL_set_connect_state(h->conn);
    SSL_set_tlsext_host_name(h->conn, hostname);
    if(SSL_connect(h->conn) != 1) {
      HTTP_LOG("Can not connect to '%s:%u' via SSL (OPEN_SSL)", hostname, port);
      return false;
    }
  }
#else
  if(use_ssl) {
    HTTP_LOG("Neither HTTP_OPEN_SSL nor HTTP_WIN32_SSL is defined. Define either to be able to use SSL.");
    return false;    
  }
#endif // HTTP_OPEN_SSL

  return true;
}

HTTP_DEF bool http_maybe_init_external_libs() {
#ifdef _WIN32
  if(!http_global_wsa_startup) {
    
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
      HTTP_LOG("Failed to initialize WSA (ws2_32.lib)\n");
      return false;
    }
    
    http_global_wsa_startup = true;
  }
#endif //_WIN32

#ifdef HTTP_OPEN_SSL
  if(!http_global_ssl_context) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    http_global_ssl_context = SSL_CTX_new(TLS_client_method());
    if(!http_global_ssl_context) {
      HTTP_LOG("Failed to initialize SSL (openssl.lib, crypto.lib)\n");
      return false;
    }    
  }
#endif //HTTP_OPEN_SSL

  return true;
}

HTTP_DEF bool http_socket_connect_plain(Http *http, const char *hostname, uint16_t port) {

#ifdef _WIN32
  struct addrinfo hints;
  struct addrinfo* result = NULL;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char port_cstr[6];
  snprintf(port_cstr, sizeof(port_cstr), "%u", port);

  if(getaddrinfo(hostname, port_cstr, &hints, &result) != 0) {
    HTTP_LOG("getaddrinfo failed");
    freeaddrinfo(result);
    return false;
  }

  bool out = true;
  if(connect(http->socket, result->ai_addr, (int) result->ai_addrlen) != 0) {
    HTTP_LOG("connect failed: %d", GetLastError());
    out = false;
  }
  
  freeaddrinfo(result);

  return out;
#elif linux
  struct sockaddr_in addr = {0};

  struct hostent *hostent = gethostbyname(hostname);
  if(!hostent) {
    return false;
  }

  in_addr_t in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
  if(in_addr == (in_addr_t) -1) {
    return false;
  }
  addr.sin_addr.s_addr = in_addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short) port);
  if(connect(http->socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    return false;
  }

  return true;
#else

  HTTP_LOG("Unsupported platform. Implement: http_socket_connect_plain");
  
  (void) http;
  (void) hostname;
  (void) port;
  return false;
#endif
}

HTTP_DEF void http_free(Http *http) {

#ifdef HTTP_OPEN_SSL
  if(http->conn) {
    SSL_set_shutdown(http->conn, SSL_RECEIVED_SHUTDOWN | SSL_SENT_SHUTDOWN);
    SSL_shutdown(http->conn);
    SSL_free(http->conn);
  }
#endif // HTTP_OPEN_SSL
  
#ifdef _WIN32
  closesocket(http->socket);
  http->socket = INVALID_SOCKET;
#elif linux
  close(http->socket);
#endif

  free((char *) http->hostname);
}

HTTP_DEF bool http_socket_write(const char *data, size_t size, void *_http) {
  
#ifdef HTTP_OPEN_SSL
  Http *http = (Http *) _http;

  if(!http->conn)
    return http_socket_write_plain(data, size, http);

  // This loop is needed, for the case that SSL_write returns the error: SSL_ERROR_WANT_WRITE.
  // If the write fails, because of any error, but we should continue trying to write.
  
  do{
    int ret = SSL_write(http->conn, data, (int) size);
    if(ret <= 0) {

      int error = SSL_get_error(http->conn, ret);

      if( error == SSL_ERROR_ZERO_RETURN ) {

	// connection was closed
	return false;
      } else if( error == SSL_ERROR_WANT_READ ) {

	// try again calling SSL_write
	continue;
      } else {

	// ssl_write error
	// TODO: maybe handle other errors
	return false;	
      }
    } else {

      // ssl_write success
      return true;
    } 

  }while(1);
#else
  (void) data;
  (void) size;
  (void) _http;

  return false;
#endif // HTTP_OPEN_SSL  
}

HTTP_DEF bool http_socket_write_plain(const char *data, size_t size, void *_http) {

  Http *http = (Http *) _http;

#ifdef _WIN32
  int ret = send(http->socket, data, (int) size, 0);
  if(ret == SOCKET_ERROR) {
    
    // send error    
    return false;
  } else if(ret == 0) {
    
    // connection was closed
    return false;
  } else {
    
    // send success
    return true;
  }
#elif linux

  int ret = send(http->socket, data, (int) size, 0);
  if(ret < 0) {
    // TODO: check if this is the right error
    if(errno == ECONNRESET) {

      // connection was closed
      return false;
    } else {

      // send error
      return false;
    }      
  } else {

    // send success
    return true;
  }
#else
  return false;
#endif 
}

HTTP_DEF bool http_socket_read(char *buffer, size_t buffer_size, void *_http, size_t *read) {

#ifdef HTTP_OPEN_SSL
  Http *http = (Http *) _http;

  if(!http->conn)
    return http_socket_read_plain(buffer, buffer_size, http, read);

  *read = 0;

  // This loop is needed, for the case that SSL_read returns the error: SSL_ERROR_WANT_READ.
  // In this case we should not close the connection which would be indicated by returning
  // a read of 0. And we should not return false, because there is still data that wants to
  // be read.
  do{

    int ret = SSL_read(http->conn, buffer, (int) buffer_size);
    if(ret < 0) {
      int error = SSL_get_error(http->conn, ret);

      if( error == SSL_ERROR_ZERO_RETURN ) {

	// connection was closed
	*read = 0;
	return false;
      } else if( error == SSL_ERROR_WANT_READ ) {

	// try again calling SSL_read
	continue;
      } else {

	// ssl_read error
	// TODO: maybe handle other errors
	return false;	
      }
    } else {

      // ssl_read success
      *read = (size_t) ret;
      return true;
    }

    break;
  }while(1);
#else
  (void) buffer;
  (void) buffer_size;
  (void) _http;
  (void) read;

  return false;
#endif // HTTP_OPEN_SSL

}

HTTP_DEF bool http_socket_read_plain(char *buffer, size_t buffer_size, void *_http, size_t *read) {

  Http *http = (Http *) _http;

#ifdef _WIN32
  int ret = recv(http->socket, buffer, (int) buffer_size, 0);
  if(ret == SOCKET_ERROR) {
    // recv error
    return false;
  } else if(ret == 0) {

    // connection was closed
    *read = 0;
    return true;
  } else {

    // recv success
    *read = (size_t) ret;
    return true;
  }
#elif linux

  int ret = recv(http->socket, buffer, (int) buffer_size, 0);
  if(ret < 0) {

    // recv error
    return false;
  } else if(ret == 0) {

    // connection was closed
    *read = 0;
    return true;
  } else {

    *read = (size_t) ret; 
    return true;
  }
#else
  return false;
#endif 
}

#define HTTP_REQUEST_STATE_DONE -2
#define HTTP_REQUEST_STATE_ERROR -1
#define HTTP_REQUEST_STATE_IDLE 0
#define HTTP_REQUEST_STATE_R    1
#define HTTP_REQUEST_STATE_RN   2
#define HTTP_REQUEST_STATE_RNR  3
#define HTTP_REQUEST_STATE_BODY 4

#define HTTP_REQUEST_PAIR_INVALID 0
#define HTTP_REQUEST_PAIR_KEY 1
#define HTTP_REQUEST_PAIR_VALUE 2

#define HTTP_REQUEST_BODY_NONE 0
#define HTTP_REQUEST_BODY_CONTENT_LEN 1
#define HTTP_REQUEST_BODY_CHUNKED 2
#define HTTP_REQUEST_BODY_INFO 3

#ifdef HTTP_OPEN_SSL
#  define HTTP_WRITE_FUNC http_socket_write
#  define HTTP_READ_FUNC http_socket_read
#else
#  define HTTP_WRITE_FUNC http_socket_write_plain
#  define HTTP_READ_FUNC http_socket_read_plain
#endif // HTTP_OPEN_SSL


HTTP_DEF bool http_request_from(Http *http, const char *route, const char *method,
				const char *headers,
				const unsigned char *body, size_t body_len,
				Http_Request *r) {

  r->http = http;
  r->buffer_size = 0;
  r->body = HTTP_REQUEST_BODY_NONE;
  r->state = HTTP_REQUEST_STATE_IDLE;
  r->state2 = HTTP_REQUEST_STATE_IDLE;
  r->pair = HTTP_REQUEST_PAIR_KEY;
  r->key_len = 0;
  r->value_len = 0;
  
  if(body_len > 0) {

    int len = (int) body_len;
    
    if(!http_sendf(HTTP_WRITE_FUNC, http, r->buffer, sizeof(r->buffer),
		   "%s %s HTTP/1.1\r\n"
		   "Host: %s\r\n"
		   "%s"
		   "Content-Length: %d\r\n"
		   "\r\n"
		   "%.*s", method, route, http->hostname, headers ? headers : "", len , len, (char *) body)) {
      HTTP_LOG("Failed to send http-request");
      return false;
    }
    
  } else {
    if(!http_sendf(HTTP_WRITE_FUNC, http, r->buffer, sizeof(r->buffer),
		   "%s %s HTTP/1.1\r\n"
		   "Host: %s\r\n"
		   "%s"
		   "\r\n", method, route, http->hostname, headers ? headers : "")) {
      HTTP_LOG("Failed to send http-request");
      return false;
    }    
  }

  if(!HTTP_READ_FUNC(r->buffer, sizeof(r->buffer), r->http, &r->buffer_size)) {
    return false;
  }
  if(r->buffer_size == 0) {
    return false;
  }
  r->buffer_pos = 0;
  
  return true;
}

HTTP_DEF bool http_next_header(Http_Request *r, Http_Header *header) {

 start:
  if(r->state == HTTP_REQUEST_STATE_ERROR ||
     r->state == HTTP_REQUEST_STATE_DONE) {
    return false;
  }

  if(r->buffer_size == 0) {
    if(!HTTP_READ_FUNC(r->buffer, sizeof(r->buffer), r->http, &r->buffer_size)) {
      return false;
    }

    if(r->buffer_size == 0) {
      return false;
    }
    r->buffer_pos = 0;
  }

  for(size_t i=0;i<r->buffer_size;i++) {
    int state_before = r->state;

    char c = r->buffer[r->buffer_pos + i];
    
    if(c == '\r') {
      if(r->state == HTTP_REQUEST_STATE_IDLE) r->state = HTTP_REQUEST_STATE_R;
      else if(r->state == HTTP_REQUEST_STATE_R) r->state = HTTP_REQUEST_STATE_IDLE;
      else if(r->state == HTTP_REQUEST_STATE_RN) r->state = HTTP_REQUEST_STATE_RNR;
      else if(r->state == HTTP_REQUEST_STATE_RNR) r->state = HTTP_REQUEST_STATE_IDLE;
      else if(r->state == HTTP_REQUEST_STATE_BODY) r->state = HTTP_REQUEST_STATE_BODY;
    } else if(c == '\n') {
      if(r->state == HTTP_REQUEST_STATE_IDLE) r->state = HTTP_REQUEST_STATE_IDLE;
      else if(r->state == HTTP_REQUEST_STATE_R) r->state = HTTP_REQUEST_STATE_RN;
      else if(r->state == HTTP_REQUEST_STATE_RN) r->state = HTTP_REQUEST_STATE_IDLE;
      else if(r->state == HTTP_REQUEST_STATE_RNR) r->state = HTTP_REQUEST_STATE_BODY;
      else if(r->state == HTTP_REQUEST_STATE_BODY) r->state = HTTP_REQUEST_STATE_BODY;
    } else {
      if(r->state == HTTP_REQUEST_STATE_BODY) r->state = HTTP_REQUEST_STATE_BODY;
      else r->state = HTTP_REQUEST_STATE_IDLE;
    }

    if(r->state == HTTP_REQUEST_STATE_IDLE && state_before == HTTP_REQUEST_STATE_RN) {
      r->pair = HTTP_REQUEST_PAIR_KEY;
    }

    if(r->pair == HTTP_REQUEST_PAIR_KEY) {
      if(c == ':') {
	r->pair = HTTP_REQUEST_PAIR_VALUE;
      } else if(c == '\r') {

	// 'HTTP/1.1 '
	static char http1_prefix[] = "HTTP";
	static size_t http1_prefix_len = sizeof(http1_prefix) - 1;

	if(r->key_len < http1_prefix_len ||
	   memcmp(http1_prefix, r->key, http1_prefix_len) != 0) {
	  HTTP_LOG("http1-prefix is not present: '%.*s'", (int) r->key_len, r->key);
	  r->state = HTTP_REQUEST_STATE_ERROR;
	  return false;
	}

	// '200'
	if(r->key_len < http1_prefix_len + 5 + 3) {
	  HTTP_LOG("http1 responseCode is not present");
	  r->state = HTTP_REQUEST_STATE_ERROR;
	  return false;
	}

        size_t out;
	if(!http_parse_u64(r->key + 5 + http1_prefix_len, 3, &out)) {
	  HTTP_LOG("Failed to parse: '%.*s'", (int) 3, r->key + http1_prefix_len + 5);
	  r->state = HTTP_REQUEST_STATE_ERROR;
	  return false;
	}
	r->response_code = (int) out;	
	
      } else if(c == '\n') {
	r->key_len = 0;
      } else {
	assert(r->key_len < sizeof(r->key) - 1);
	r->key[r->key_len++] = c;
      }
    } else if(r->pair == HTTP_REQUEST_PAIR_VALUE) {
      if(c == '\r') {

	static char content_length_cstr[] = "content-length";
	static size_t content_length_cstr_len = sizeof(content_length_cstr) - 1;

	if(http_header_eq(r->key, r->key_len, content_length_cstr, content_length_cstr_len)) {

	  size_t len = r->value_len - 1;
	  if(!http_parse_u64(r->value, len, &r->content_length)) {
	    HTTP_LOG("Failed to parse: '%.*s'", (int) len, r->value);
	    r->state = HTTP_REQUEST_STATE_ERROR;
	    return false;
	  }

	  if(r->body != HTTP_REQUEST_BODY_NONE) {
	    HTTP_LOG("Http-Body was already specified");
	    r->state = HTTP_REQUEST_STATE_ERROR;
	    return false;
 
	  }
	  r->body = HTTP_REQUEST_BODY_CONTENT_LEN;
	  r->content_read = 0;
	}

	static char chunked_encoding[] = "transfer-encoding";
	static size_t chunked_encoding_len = sizeof(chunked_encoding) - 1;
	static char chunked[] = "chunked";
	static size_t chunked_len = sizeof(chunked) - 1;
	
	if(http_header_eq(r->key, r->key_len, chunked_encoding, chunked_encoding_len) &&
	   http_header_eq(r->value, r->value_len - 1, chunked, chunked_len)) {
	  if(r->body != HTTP_REQUEST_BODY_NONE) {
	    HTTP_LOG("Http-Body was already specified");
	    r->state = HTTP_REQUEST_STATE_ERROR;
	    return false;
 
	  }
	  r->body = HTTP_REQUEST_BODY_CHUNKED;
	  r->content_length = 0;
	  r->content_read = 0;
	}

        r->key[r->key_len] = 0;
	r->value[r->value_len - 1] = 0;

	header->key = r->key;
	header->key_len = r->key_len;
	header->value = r->value;
	header->value_len = r->value_len - 1;
	
	r->pair = HTTP_REQUEST_PAIR_INVALID;
	r->value_len = 0;
	r->key_len = 0;

	r->buffer_pos  += i - 1;
	r->buffer_size -= i - 1;
        return true;
      } else {
	assert(r->value_len < sizeof(r->value) - 1);
	if(r->value_len == 0) r->value_len++;
	else r->value[r->value_len++ - 1] = c;
      }
    }

    if(r->state == HTTP_REQUEST_STATE_BODY) {
      r->buffer_pos  += i + 1;
      r->buffer_size -= i + 1;
      return false;
    }

  }

  r->buffer_size = 0;
  goto start;
}

HTTP_DEF bool http_next_body(Http_Request *r, char **data, size_t *data_len) {
  
  // Maybe parse Headers
  if(r->state != HTTP_REQUEST_STATE_BODY) {
    Http_Header header;
    while(http_next_header(r, &header)) ;
  }

 start:

  // Mabye exit
  if(r->state == HTTP_REQUEST_STATE_ERROR ||
     r->state == HTTP_REQUEST_STATE_DONE) {
    return false;
  }
 
  // Read 
  if(r->buffer_size == 0) {
    if(!HTTP_READ_FUNC(r->buffer, sizeof(r->buffer), r->http, &r->buffer_size)) {
      return false;
    }

    if(r->buffer_size == 0) {
      return false;
    }
    r->buffer_pos = 0;
  }

  // Consume
  if(r->body == HTTP_REQUEST_BODY_CONTENT_LEN) {

    if(r->content_read + r->buffer_size > r->content_length) {
      HTTP_LOG("Server send too much data");
      return false;
    }
    r->content_read += r->buffer_size;
    
    if(r->content_read == r->content_length) {
      r->state = HTTP_REQUEST_STATE_DONE;
    }
    
    *data = r->buffer + r->buffer_pos;
    *data_len = r->buffer_size;
    r->buffer_size = 0;

    return true;
  } else if(r->body == HTTP_REQUEST_BODY_CHUNKED) {

    for(size_t i=0;i<r->buffer_size;i++) {
      char c = r->buffer[r->buffer_pos + i];

      if(c == '\r') {
	r->state2 = HTTP_REQUEST_STATE_R;
      } else if(c == '\n') {
	if(r->state2 == HTTP_REQUEST_STATE_R) r->state2 = HTTP_REQUEST_STATE_RN;
	else r->state2 = HTTP_REQUEST_STATE_IDLE;
      } else {
	r->state2 = HTTP_REQUEST_STATE_IDLE;
      }

      if(r->content_read == 0) {
	if(r->state2 == HTTP_REQUEST_STATE_IDLE) {
	  assert(r->key_len < 4);
	  r->key[r->key_len++] = c;
	} else if(r->state2 == HTTP_REQUEST_STATE_RN) {

	  // TODO: this may be incorrect
	  if(r->key_len == 0) {
	    /* HTTP_LOG("Failed to parse: '%.*s'", (int) r->key_len, r->key); */
	    /* r->state = HTTP_REQUEST_STATE_ERROR; */
	    /* return false; */

	    continue;
	  }

	  if(!http_parse_hex_u64(r->key, r->key_len, &r->content_read)) {
	    HTTP_LOG("Failed to parse: '%.*s'", (int) r->key_len, r->key);
	    r->state = HTTP_REQUEST_STATE_ERROR;
	    return false;
	  }
	  r->key_len = 0;

	  size_t advance = i + 1; // consume '\n'
	  
	  r->buffer_pos += advance;
	  r->buffer_size -= advance;
	  if(r->content_read == 0) {
	    r->state = HTTP_REQUEST_STATE_DONE;
	    return false;
	  } else {
	    goto start;
	  }
	  
	} else {
	  // parse \r\n
	}	      
      } else {

	if(r->state2 == HTTP_REQUEST_STATE_RN) {
	  
	  size_t len = i - 1;      // exclude '\r'
	  size_t advance = i + 1;  // consume '\n'

	  *data = r->buffer + r->buffer_pos;
	  *data_len = len;

	  r->buffer_pos += advance;
	  r->buffer_size -= advance;

	  assert(r->content_read >= len);
	  r->content_read -= len;
	  r->content_length += len;
	  return true;
	} else {
	  //do nothing
	}	  
      }
      
    }

    if(r->content_read == 0) {
      r->buffer_size = 0;
      
      goto start;
    } else {

      size_t len = r->buffer_size;
      if(r->state2 == HTTP_REQUEST_STATE_R) {
	len--;
      }

      *data = r->buffer + r->buffer_pos;
      *data_len = len;

      assert(r->content_read >= len);
      r->content_read -= len;
      r->content_length += len;

      r->buffer_pos += r->buffer_size;
      r->buffer_size -= r->buffer_size;
      
      return true;
    }
      

  } else {
    
    HTTP_LOG("Unimplemented body specification");
    return false;
  }
    
}

HTTP_DEF bool http_parse_hex_u64(char *buffer, size_t buffer_len, uint64_t *out) {
  size_t i = 0;
  uint64_t res = 0;

  while(i < buffer_len) {
    char c = buffer[i];

    res *= 16;
    if('0' <= c && c <= '9') {
      res += c - '0';
    } else if('a' <= c && c <= 'z') {
      res += c - 'W';
    } else if('A' <= c && c <= 'Z') {
      res += c - '7';
    } else {
      break;
    }
    i++;
  }

  *out = res;
  
  return i > 0 && i == buffer_len;
}

HTTP_DEF bool http_parse_u64(char *buffer, size_t buffer_len, uint64_t *out) {

  uint64_t res = 0;

  size_t i = 0;
  while(i < buffer_len && '0' <= buffer[i] && buffer[i] <= '9') {
    res *= 10;
    res += buffer[i] - '0';
    i++;
  }

  *out = res;
  
  return i > 0 && i == buffer_len;
}

// key  : 'ConTENT-LeNGTHasdfasdfasdf'
// value: 'content-length'
//     => true
HTTP_DEF bool http_header_eq(const char *key, size_t key_len, const char *value, size_t value_len) {

  if(key_len != value_len) {
    return false;
  }

  for(size_t i=0;i<key_len;i++) {
    char src = value[i];
    char trg = key[i];

    if('a' <= src && src <= 'z' &&
       'A' <= trg && trg <= 'Z')  {
      trg += ' ';
    }

    if(src != trg) {
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////


HTTP_DEF bool http_sendf(Http_Sendf_Callback send_callback, void *userdata,
			 char *buffer, size_t buffer_cap, const char *format, ...) {
  va_list va;
  va_start(va, format);
  bool result = http_sendf_impl(send_callback, userdata, buffer, buffer_cap, format, va);
  va_end(va);
  return result;
}

HTTP_DEF bool http_sendf_impl(Http_Sendf_Callback send_callback, void *userdata,
			      char *buffer, size_t buffer_cap, const char *format, va_list va) {
  Http_Sendf_Context context = {0};
  context.send_callback = send_callback;
  context.buffer = buffer;
  context.buffer_cap = buffer_cap;
  context.userdata = userdata;
  context.last = false;

  size_t buffer_size = 0;
  size_t format_len = strlen(format);
  size_t format_last = 0;

  for(size_t i=0;i<format_len;i++) {
    if(format[i]=='%' && i+1 < format_len) {
      if(!http_sendf_impl_send(&context, &buffer_size, format + format_last, i - format_last)) {
	return false;
      }
      if (format[i+1] == 'c') { // %c
	char c = (char) va_arg(va, int);
	if(!http_sendf_impl_send(&context, &buffer_size, &c, 1)) {
	  return false;
	}

	format_last = i+2;
	i++;
      } else if(format[i+1]=='s') { // %
	const char *argument_cstr = va_arg(va, char *);
	if(!http_sendf_impl_send(&context, &buffer_size, argument_cstr, strlen(argument_cstr))) {
	  return false;
	}

	format_last = i+2;
	i++;
      } else if(format[i+1]=='d') { // %d
	int n = va_arg(va, int);

	if(n == 0) {
	  const char *zero = "0";
	  if(!http_sendf_impl_send(&context, &buffer_size, zero, 1)) {
	    return false;
	  }	  
	} else {
#define HTTP_SENDF_DIGIT_BUFFER_CAP 32
	  static char digit_buffer[HTTP_SENDF_DIGIT_BUFFER_CAP ];
	  size_t digit_buffer_count = 0;
	  bool was_negative = false;
	  if(n < 0) {
	    was_negative = true;
	    n *= -1;
	  }
	  while(n > 0) {
	    int m = n % 10;
	    digit_buffer[HTTP_SENDF_DIGIT_BUFFER_CAP - digit_buffer_count++ - 1] = (char) m + '0';
	    n = n / 10;
	  }
	  if(was_negative) {
	    digit_buffer[HTTP_SENDF_DIGIT_BUFFER_CAP - digit_buffer_count++ - 1] = '-';
	  }
	  if(!http_sendf_impl_send(&context, &buffer_size,
				   digit_buffer + (HTTP_SENDF_DIGIT_BUFFER_CAP - digit_buffer_count), digit_buffer_count)) {
	    return false;
	  }
	}	

	format_last = i+2;
	i++;
      } else if(format[i+1] == '.' && i+3 < format_len &&
		format[i+2] == '*' && format[i+3] == 's') { //%.*s

	int argument_cstr_len = va_arg(va, int);
	const char *argument_cstr = va_arg(va, char *);

	if(!http_sendf_impl_send(&context, &buffer_size, argument_cstr, (size_t) argument_cstr_len)) {
	  return false;
	}

	format_last = i+4;
	i+=3;
      }
    }
  }

  context.last = true;
  if(!http_sendf_impl_send(&context, &buffer_size, format + format_last, format_len - format_last)) {
    return false;
  }

  return true;
}

HTTP_DEF size_t http_sendf_impl_send(Http_Sendf_Context *context, size_t *buffer_size, const char *cstr, size_t cstr_len) {
  size_t cstr_off = 0;
  while(true) {
    *buffer_size = http_sendf_impl_copy(context, *buffer_size, cstr, cstr_len, &cstr_off);
    if(*buffer_size == context->buffer_cap || (context->last && *buffer_size != 0)) {
      if(!context->send_callback(context->buffer, *buffer_size, context->userdata)) {
	return false;
      }
    }
    if(*buffer_size < context->buffer_cap) break;
    *buffer_size = 0;
  }

  return true;
}

HTTP_DEF size_t http_sendf_impl_copy(Http_Sendf_Context *context, size_t buffer_size,
				     const char *cstr, size_t cstr_len, size_t *cstr_off) {
  size_t diff = cstr_len - *cstr_off;

  if(buffer_size + diff < context->buffer_cap) {
    memcpy(context->buffer + buffer_size, cstr + *cstr_off, diff);

    *cstr_off = 0;
    return buffer_size + diff;
  } else{
    size_t buffer_diff = context->buffer_cap - buffer_size;
    memcpy(context->buffer + buffer_size, cstr + *cstr_off, buffer_diff);
    
    (*cstr_off) += buffer_diff;
    return buffer_size + buffer_diff;
  }  

}

#endif // HTTP_IMPLEMENTATION

#endif // HTTP_H
