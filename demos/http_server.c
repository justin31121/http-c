#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>

#define CLIENTS_CAP 100

#define READ_BUFFER_CAP 4096
#define NAME_CAP 512

typedef enum{
    STATE_ERROR,
    STATE_IDLE, //
    STATE_R,    // \r
    STATE_RN,   // \r\n
    STATE_RNR,  // \r\n\r
    STATE_FINAL,// \r\n\r\n
}State;

typedef enum{
    STATE_ROW_ERROR,
    STATE_ROW_IDLE, // 
    STATE_ROW_KEY,    // Connection:
    STATE_ROW_VALUE,  // Connection:Close
}State_Row;

typedef struct{
    char names[2][NAME_CAP];
    size_t names_size[2]; // = {0};

    char method[NAME_CAP];
    size_t method_size; // = 0;

    char path[NAME_CAP];
    size_t path_size; // 0;

    State state; // STATE_IDLE
    State_Row state_row; // STATE_ROW_KEY

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
    c->row = 0;
}

Ret context_forward(Client_Context *c, const char *data, size_t size) {
    for(size_t i=0;i<size;i++) {
	State before = c->state;
	if(data[i] == '\r') {
	    if(c->state == STATE_IDLE) c->state = STATE_R;
	    else if(c->state == STATE_R) c->state = STATE_IDLE;
	    else if(c->state == STATE_RN) c->state = STATE_RNR;
	    else if(c->state == STATE_RNR) c->state = STATE_IDLE;
	    else if(c->state == STATE_FINAL) c->state = STATE_FINAL;
	} else if(data[i] == '\n') {
	    if(c->state == STATE_IDLE) c->state = STATE_IDLE;
	    else if(c->state == STATE_R) c->state = STATE_RN;
	    else if(c->state == STATE_RN) c->state = STATE_IDLE;
	    else if(c->state == STATE_RNR) c->state = STATE_FINAL;
	    else if(c->state == STATE_FINAL) c->state = STATE_FINAL;
	} else {
	    if(c->state == STATE_IDLE) c->state = STATE_IDLE;
	    else if(c->state == STATE_R) c->state = STATE_IDLE;
	    else if(c->state == STATE_RN) c->state = STATE_IDLE;
	    else if(c->state == STATE_RNR) c->state = STATE_IDLE;
	    else if(c->state == STATE_FINAL) c->state = STATE_FINAL;
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

	if(c->state == STATE_ERROR) return RET_ABORT;
	if(c->state_row == STATE_ROW_ERROR) return RET_ABORT;
    }
    
    return c->state == STATE_FINAL ? RET_SUCCESS : RET_CONTINUE;
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

#define NOT_FOUND \
    "HTTP/1.1 404 Not Found\r\n"		\
    "Connection: keep-alive\r\n"		\
    "Content-Type: text/plain\r\n"		\
    "Content-Length: 11\r\n"			\
    "\r\n"					\
    "Not Found !"

int handle_request(SOCKET client, Client_Context *c) {

    // 0  indicates disconnection
    // >0 indicates success
    // <0 indicates error

    static char *dir = "/rsc";
    
    const char *suffix = "inde.html";    
    if( strcmp(c->path, "/") != 0 )  {
	
	memcpy( filepath, dir, sizeof(dir)-1 );
	mempcy( filepath + sizeof(dir))
    }
    
    

    FILE *f = fopen(filepath, "rb");
    if(!f) {
	return send_loop(client, NOT_FOUND, strlen(NOT_FOUND), 0);
    }

    fclose(f);
    
    return 0;
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
    addr_in.sin_port = htons(8079);

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

		char buf[READ_BUFFER_CAP];
		int ret_recv = recv(clients[i], buf, sizeof(buf), 0);
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

		    Ret ret_forward = context_forward(&contexts[i], buf, (size_t) ret_recv);
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
