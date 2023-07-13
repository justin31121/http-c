#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>

#define CLIENTS_CAP 100

#define READ_BUFFER_CAP 8192
#define NAME_CAP 512

typedef enum{
  STATE_ERROR,
  STATE_IDLE, //
  STATE_R,    // \r
  STATE_RN,   // \r\n
  STATE_RNR,  // \r\n\r
  STATE_BODY,// \r\n\r\n
}State;

typedef enum{
  STATE_ROW_ERROR,
  STATE_ROW_IDLE, // 
  STATE_ROW_KEY,    // Connection:
  STATE_ROW_VALUE,  // Connection:Close
}State_Row;

typedef enum{
  STATE_LEN_ERROR,
  STATE_LEN_UNKNOWN,
  STATE_LEN_FIX,
  STATE_LEN_CHUNKED,
  STATE_LEN_CHUNKED_COLLECT,
}State_Len;

typedef struct{
  char names[2][NAME_CAP];
  size_t names_size[2]; // = {0};

  char method[NAME_CAP];
  size_t method_size; // = 0;

  char path[NAME_CAP];
  size_t path_size; // 0;

  State state; // STATE_IDLE
  State_Row state_row; // STATE_ROW_KEY
  State_Len state_len; // STATE_LEN_UNKNOWN

  int len;
  int need;
  int row;
}Client_Context;

typedef enum{
  RET_ABORT = 0,
  RET_CONTINUE = 1,
  RET_SUCCESS = 2,
}Ret;

void context_clear(Client_Context *c) {
  memset(c->names_size, 0, sizeof(size_t) * 2);
  c->method_size = 0;
  c->path_size = 0;
  c->state = STATE_IDLE;
  c->state_row = STATE_ROW_KEY;
  c->state_len = STATE_LEN_UNKNOWN;
  c->row = 0;
  c->need = 0;
}

Ret context_forward(Client_Context *c, const char *data, size_t size) {
  for(size_t i=0;i<size;i++) {
    State before = c->state;
    if(data[i] == '\r') {
      if(c->state == STATE_IDLE) c->state = STATE_R;
      else if(c->state == STATE_R) c->state = STATE_IDLE;
      else if(c->state == STATE_RN) c->state = STATE_RNR;
      else if(c->state == STATE_RNR) c->state = STATE_IDLE;
      else if(c->state == STATE_BODY) c->state = STATE_BODY;
    } else if(data[i] == '\n') {
      if(c->state == STATE_IDLE) c->state = STATE_IDLE;
      else if(c->state == STATE_R) c->state = STATE_RN;
      else if(c->state == STATE_RN) c->state = STATE_IDLE;
      else if(c->state == STATE_RNR) c->state = STATE_BODY;
      else if(c->state == STATE_BODY) c->state = STATE_BODY;
    } else {
      if(c->state == STATE_IDLE) c->state = STATE_IDLE;
      else if(c->state == STATE_R) c->state = STATE_IDLE;
      else if(c->state == STATE_RN) c->state = STATE_IDLE;
      else if(c->state == STATE_RNR) c->state = STATE_IDLE;
      else if(c->state == STATE_BODY) c->state = STATE_BODY;
    }

    if(before == STATE_RN && c->state == STATE_IDLE) {
      c->names_size[0] = 0;
      c->state_row = STATE_ROW_KEY;
    }
    else if(c->state == STATE_IDLE && c->state_row == STATE_ROW_KEY && data[i] == ':') {
      c->names_size[1] = 0;
      c->state_row = STATE_ROW_VALUE;
    }
    else if(before == STATE_IDLE && c->state == STATE_R) {

      if(c->row == 0) {
	int space_location = -1;
	size_t j=0;
	for(;j<c->names_size[0];j++) {
	  if(c->names[0][j] == ' ') {space_location = (int) j; break; }
	}
	if(space_location < 0) {
	  //no method, no path
	  return RET_ABORT;
	}
	memcpy(c->method, c->names[0], space_location);
	c->method_size = space_location;
	c->method[c->method_size] = 0;

	space_location = -1;
	for(size_t k=j+1;k<c->names_size[0];k++) {
	  if(c->names[0][k] == ' ') {space_location = (int) k; break; }
	}
	if(space_location < 0) {
	  // no path
	  return RET_ABORT;
	}
	memcpy(c->path, c->names[0] + j+1, space_location - j - 1);
	c->path_size = space_location - j - 1;
	c->path[c->path_size] = 0;

	//printf("method: '%.*s'\n", (int) c->method_size, c->method);
	//printf("path: '%.*s'\n", (int) c->path_size, c->path);
				
      } else {
	/*printf("key: '%.*s', value: '%.*s', row: %d\n",
	  (int) c->names_size[0], c->names[0],
	  c->names_size[1] ? (int) c->names_size[1] - 2 : 0, c->names[1] + 2,
	  c->row);*/
	if( (strncmp(c->names[0], "content-length", c->names_size[0]) == 0) ||
	    (strncmp(c->names[0], "Content-Length", c->names_size[0]) == 0)) {

	  c->len = 0;
	  for(int j=2;j<c->names_size[1];j++) {
	    c->len *= 10;
	    char f = c->names[1][j];
	    if(f < '0' || f > '9') return RET_ABORT;
	    c->len += f - '0';
	  }
	  c->need = c->len+1;
	  if(c->state_len != STATE_LEN_UNKNOWN) return RET_ABORT;
	  c->state_len = STATE_LEN_FIX;
	  
	}
	
      }
      c->state_row = STATE_ROW_IDLE;
      c->row++;
	    
      c->names_size[0] = 0;
      c->names_size[1] = 0;
    }

    if(c->state_row == STATE_ROW_KEY) {
      assert(c->names_size[0] < NAME_CAP);
      c->names[0][c->names_size[0]++] = data[i];
    } else if(c->state_row == STATE_ROW_VALUE) {
      assert(c->names_size[1] < NAME_CAP);
      c->names[1][c->names_size[1]++] = data[i];
    }

    if(c->state == STATE_BODY) {
      if(c->state_len == STATE_LEN_UNKNOWN) ; // request without body
      else if(c->state_len == STATE_LEN_FIX) c->need--; // consume c->need, with every char
      else c->state_len = STATE_LEN_ERROR; // FOR NOW, THROW ERROR
    }

    if(c->state == STATE_ERROR) return RET_ABORT;
    if(c->state_row == STATE_ROW_ERROR) return RET_ABORT;
    if(c->state_len == STATE_LEN_ERROR) return RET_ABORT;
  }

  if(c->state == STATE_BODY && c->need == 0) return RET_SUCCESS;
  return RET_CONTINUE;
}

int send_loop(SOCKET s, const char *buf, int buf_len, int flags) {

  // 0  indicates disconnection
  // >0 indicates success
  // <0 indicates error
    
  int len = 0;
    
  while(len < buf_len) {
    int ret = send(s, buf + len, buf_len - len, flags);
    if(ret == SOCKET_ERROR) {
      // send error
      // TODO: maybe try again and handle the error ?
      return ret;
    } else if(ret == 0) {
      // connection was closed
      return ret;
    } else {
      // send success
      len += ret;
    }
  }
    
  return len;
}

#define NOT_FOUND				\
  "HTTP/1.1 404 Not Found\r\n"			\
  "Connection: keep-alive\r\n"			\
  "Content-Type: text/plain\r\n"		\
  "Content-Length: 11\r\n"			\
  "\r\n"					\
  "Not Found !"

#define INTERNAL_ERROR				\
  "HTTP/1.1 500 Internal Server Error\r\n"	\
  "Connection: keep-alive\r\n"			\
  "Content-Type: text/plain\r\n"		\
  "Content-Length: 21\r\n"			\
  "\r\n"					\
  "Internal Server Error"

const char *mime_type(const char *cstr, size_t len) {
  size_t i;
  for(i=len-1;i>=0;i--) {
    if(cstr[i] == '.') {
      i++; break;
    }
  }

  if( strncmp(cstr + i, "html", len - i) == 0 ) {
    return "text/html";
  } else if( strncmp(cstr + i, "js", len - i) == 0 ) {
    return "application/javascript";
  } else {
    return "text/plain";
  }

}

static char temp_buffer[READ_BUFFER_CAP];

int file_handler(SOCKET client, Client_Context *c) {
 
  static char dir[] = "rsc";
  static char default_path[] = "/index.html";

  char filepath[MAX_PATH];
  size_t size = 0;
  if( strcmp(c->path, "/") == 0 ) {
    memcpy(filepath + size, dir, sizeof(dir)-1);
    size += sizeof(dir)-1;
    memcpy(filepath + size, default_path, sizeof(default_path)-1);
    size += sizeof(default_path)-1;
    filepath[size] = 0;
  } else {
    memcpy(filepath + size, dir, sizeof(dir)-1);
    size += sizeof(dir)-1;
    memcpy(filepath + size, c->path, c->path_size);
    size += c->path_size;
    filepath[size] = 0;
  }

  FILE *f = fopen(filepath, "rb");
  if(!f) {
    return send_loop(client, NOT_FOUND, sizeof(NOT_FOUND)-1, 0);  
  }

  int n;
  if((n = snprintf(temp_buffer, READ_BUFFER_CAP-1,
		   "HTTP/1.1 200 Ok\r\n"
		   "Connection: keep-alive\r\n"
		   "Content-Type: %s\r\n"
		   "Transfer-Encoding: chunked\r\n"
		   "\r\n", mime_type(filepath, size))) >= READ_BUFFER_CAP) {
    return -1;
  }

  if((n = send_loop(client, temp_buffer, n, 0)) <= 0) {
    return n;
  }

  int acc = 0;  
  size_t off = 6;
  char *buffer_start = temp_buffer;
  char *buffer_off   = temp_buffer + off;
  size_t cap = READ_BUFFER_CAP - 2 - off;

  for(;;) {
    size_t m = fread(buffer_off, 1, cap, f);
    if(m != cap) {
      if(!feof(f)) {
	return send_loop(client, INTERNAL_ERROR, sizeof(INTERNAL_ERROR)-1, 0);
      }
    }

    int temp = m;
    int k = 0;
    while(m) {
      char f = (char) (m % 16);
      temp_buffer[3 - (k++)] = f<10 ? f + '0' : f + 'W';
      m /= 16;
    }
    m = temp;

    buffer_start[4] = '\r';
    buffer_start[5] = '\n';
    buffer_off[m] = '\r';
    buffer_off[m+1] = '\n';

    if( (n = send_loop(client, buffer_start + (4 - k), k + 2 + (int) m + 2, 0)) <= 0) {
      return n;
    }
    acc += n;
    
    if(m != cap) {
      
      static char eof[] = { '0', '\r', '\n', '\r', '\n'};
      if( (n = send_loop(client, eof, sizeof(eof), 0)) <= 0) {
	return n;
      }
      acc += n;

      
      break;
    }
  }  
 
  fclose(f);

  return acc;
  
}

int handle_request(SOCKET client, Client_Context *c) {

  // 0  indicates disconnection
  // >0 indicates success
  // <0 indicates error

  if( strcmp(c->method, "GET") == 0) {
    return file_handler(client, c);
  }

  const char *mess = "Needs to be implemented";

  int n;
  if((n = snprintf(temp_buffer, READ_BUFFER_CAP-1,
		   "HTTP/1.1 200 Ok\r\n"
		   "Connection: keep-alive\r\n"
		   "Content-Type: text/plain\r\n"
		   "Content-Length: %zu\r\n"
		   "\r\n"
		   "%s", strlen(mess), mess)) >= READ_BUFFER_CAP) {
    return -1;
  }

  return send_loop(client, temp_buffer, n, 0);
}    

int main() {

  WSADATA wsaData;
  if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
    fprintf(stderr, "ERROR: WSAStartup error\n");
    return 1;
  }

  SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
  if( socket == INVALID_SOCKET ) {
    fprintf(stderr, "ERROR: WSASocketW error\n");
    return 1;
  }

  struct sockaddr_in addr_in;
  addr_in.sin_family = AF_INET;
  addr_in.sin_addr.s_addr = INADDR_ANY;
  addr_in.sin_port = htons(4200);

  if(bind(socket, (SOCKADDR*) &addr_in, sizeof(addr_in)) == SOCKET_ERROR) {
    fprintf(stderr, "ERROR: bind error\n");
    return 1;
  }

  if(listen(socket, CLIENTS_CAP) < 0) {
    fprintf(stderr, "ERROR: listen error\n");
    return 1;
  }

  SOCKET clients[CLIENTS_CAP];
  Client_Context contexts[CLIENTS_CAP];
  for(int i=0;i<CLIENTS_CAP;i++) {
    clients[i] = INVALID_SOCKET;
  }

  fd_set fds;
  while(1) {

    // clear fd
    FD_ZERO(&fds);
    // associate server-socket with fd
    FD_SET(socket, &fds);

    // associate all valid client-sockets with fd
    for(int i=0;i<CLIENTS_CAP;i++) {
      if(clients[i] != INVALID_SOCKET) {
	FD_SET(clients[i], &fds);
      }
    }

    struct timeval timeout;
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;

    int ret_select = select(0, &fds, NULL, NULL, &timeout);
    if(ret_select == SOCKET_ERROR) {
      // select error
      fprintf(stderr, "ERROR: select error\n");
      return 1;
    } else if(ret_select == 0) {
      // disconnect all valid clients, if timeout exceeds

      for(int i=0;i<CLIENTS_CAP;i++) {
		
	if(clients[i] == INVALID_SOCKET) {
	  continue;
	}
		
	printf("Client %d timed out from slot %d\n",
	       (int) clients[i], i);

	closesocket(clients[i]);
	clients[i] = INVALID_SOCKET;
		
      }
      continue;
    }

    // if socket in fd, then accept client
    if(FD_ISSET(socket, &fds)) {
      for(int i=0;i<CLIENTS_CAP;i++) {
	if(clients[i] == INVALID_SOCKET) {
	  clients[i] = accept(socket, NULL, NULL);
	  if(clients[i] == INVALID_SOCKET) {
	    // accept error
	    fprintf(stderr, "ERROR: accept error\n");
	    return 1;
	  }

	  printf("Client %d connected at slot %d\n",
		 (int) clients[i], i);

	  // Clear context
	  context_clear(&contexts[i]);
	  break;
	}
      }
    }
	
    // check if a client is in fd
    for(int i=0;i<CLIENTS_CAP;i++) {
	    
      if(clients[i] == INVALID_SOCKET) {
	continue;
      }

      if(FD_ISSET(clients[i], &fds)) {

	int ret_recv = recv(clients[i], temp_buffer, READ_BUFFER_CAP, 0);
	if(ret_recv == SOCKET_ERROR) {
	  // recv error
	  fprintf(stderr, "ERROR: recv error\n");
	  return 1;
	} else if(ret_recv == 0) {
	  // connection was closed

	  printf("Client %d disconnected from slot %d\n",
		 (int) clients[i], i);
		    
	  closesocket(clients[i]);
	  clients[i] = INVALID_SOCKET;
	} else {
	  // recv success

	  printf("Received: %d bytes\n", ret_recv);

	  Ret ret_forward = context_forward(&contexts[i], temp_buffer, (size_t) ret_recv);
	  if(ret_forward == RET_ABORT) {
	    // protocol error, disconnect connection

	    printf("Client %d timed out from slot %d\n",
		   (int) clients[i], i);

	    closesocket(clients[i]);
	    clients[i] = INVALID_SOCKET;
	  } else if (ret_forward == RET_SUCCESS) {
	    // answer request
	    int ret_response = handle_request(clients[i], &contexts[i]);
	    if(ret_response < 0) {
	      // error in response, disconnect connection
			    
	      printf("Client %d timed out from slot %d\n",
		     (int) clients[i], i);

	      closesocket(clients[i]);
	      clients[i] = INVALID_SOCKET;
	    } else if(ret_response == 0) {
	      // connection was closed

	      printf("Client %d disconnected from slot %d\n",
		     (int) clients[i], i);
		    
	      closesocket(clients[i]);
	      clients[i] = INVALID_SOCKET;
	    } else {
	      // all good, keep connection
	    }

	    // Clear context
	    context_clear(&contexts[i]);
	  } else {
	    // continue receiving more data
	  }	    	 	    
	}

		
      }
    }
  }

  closesocket(socket);
  WSACleanup();
    
  return 0;
}
