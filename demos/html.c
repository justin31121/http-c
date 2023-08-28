#include <stdio.h>

#define HTML_PARSER_BUFFER_CAP (1024 * 2)

#define HTML_PARSER_IMPLEMENTATION
#define HTML_PARSER_VERBOSE
#include "../src/html_parser.h"

#define IO_IMPLEMENTATION
#include "../src/io.h"

#define CHECK_HTML(payload) do{						\
	Html_Parser parser = html_parser(NULL, NULL, NULL, NULL, NULL);	\
	Html_Parser_Ret ret =						\
	    html_parser_consume(&parser, (payload), strlen((payload)));	\
	if(ret == HTML_PARSER_RET_ABORT) {				\
	    fprintf(stderr, "\"%s\" => ABORT\n", payload);		\
	    return 1;							\
	} else if(ret == HTML_PARSER_RET_CONTINUE) {			\
	    fprintf(stderr, "\"%s\" => CONTINUE\n", payload);		\
	    return 1;							\
	} else {							\
	    printf("\"%s\" => Passed!\n", payload);			\
	}								\
    }while(0)


int counter = 0;

typedef struct Content Content;
typedef struct Attribute Attribute;
typedef struct Tag Tag;

struct Content{
    char *text;
    size_t text_len;
    Tag *tag;
    Content *next;
};

struct Attribute {
    char *key;
    char *value;
    Attribute *next;
};

struct Tag {
    char *name;
    int id;
    Attribute *attrs;
    Content *content;
};

bool on_node(const char *name, void *arg, void **node) {

    Tag **root = arg;

    Tag *tag = malloc(sizeof(Tag));
    size_t name_len = strlen(name) + 1;
    tag->name = malloc(name_len);
    tag->id = counter++;
    tag->attrs = NULL;
    tag->content = NULL;
    memcpy(tag->name, name, name_len);
    
    *node = tag;

    if((*root) == NULL) {
	*root = tag;
    }

    
    return true;
}

bool on_node_attribute(void *node, const char *key, const char *value, void *arg) {

    Tag *tag = node;
    
    Attribute *attr = malloc(sizeof(Attribute));
    attr->next = NULL;
    
    size_t len = strlen(key) + 1;
    attr->key = malloc(len);
    memcpy(attr->key, key, len);
    
    len = strlen(value) + 1;
    attr->value = malloc(len);
    memcpy(attr->value, value, len);

    if(!tag->attrs) {
	tag->attrs = attr;
    } else {
	Attribute *a = tag->attrs;
	while(a->next) {
	    a = a->next;
	}
	a->next = attr;
    }    

    return true;
}

bool on_node_child(void *node, void *_child, void *arg) {

    Tag *parent = node;
    Tag *child = _child;

    Content *content = malloc(sizeof(Content));
    content->tag = child;
    content->next = NULL;
    content->text = NULL;

    if(!parent->content) {
	parent->content = content;
    } else {
	Content *a = parent->content;
	while(a->next) {
	    a = a->next;
	}
	a->next = content;
    }
    
    //printf("%s(%d) -> %s(%d)\n", parent->name, parent->id, child->name, child->id);
    
    return true;
}

bool on_node_content(void *node, const char *content, size_t content_size, void *arg) {

    Tag *tag = node;
    printf("tag: name: %s\n", tag->name);
    Content *c = malloc(sizeof(Content));
    c->tag = NULL;
    c->text_len = content_size;
    c->next = NULL;
    c->text = malloc(sizeof(char) * content_size);
    memcpy(c->text, content, content_size);

    if(!tag->content) {
        tag->content = c;
    } else {
	Content *a = tag->content;
	while(a->next) {
	    a = a->next;
	}
	a->next = c;
    }
    
    return true;
}

void print(Tag *tag, int depth) {

    for(int i=0;i<depth;i++) printf("\t");
    printf("<%s ", tag->name);
    Attribute *a = tag->attrs;
    while(a) {
	const char *key = a->key;
	const char *value = a->value;
	if(strlen(value) > 0) {
	    printf("%s=\"%s\" ", key, value);	    
	} else {
	    printf("%s ", key);
	}
	a = a->next;
    }
    printf(">\n");

    Content *c = tag->content;
    while(c) {
	if(c->text != NULL) {
	    for(int i=0;i<depth;i++) printf("\t");
	    printf("%.*s\n", (int) c->text_len, c->text);
	} else {
	    print(c->tag, depth + 1);	    
	}
	c = c->next;
    }

    for(int i=0;i<depth;i++) printf("\t");
    printf("</%s>\n", tag->name);
}

int main() {

    char *buffer;
    size_t buffer_size;
    if(!io_slurp_file("rsc/youtube.html", &buffer, &buffer_size)) {
	return 1;
    }

    Tag *root = NULL;
    Html_Parser parser = html_parser(on_node, on_node_attribute, on_node_child, on_node_content, &root);

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

	    print(root, 0);
	
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
