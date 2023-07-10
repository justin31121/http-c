#ifndef JSON_H
#define JSON_H

#include <stdint.h>

#ifdef JSON_IMPLEMENTATION
#  define HASHTABLE_IMPLEMENTATION
#endif //JSON_IMPLEMENTATION

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

typedef struct{
  void *data;
  uint64_t len;
  uint64_t cap;
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

#define HASHTABLE_VALUE_TYPE Json

// hashtable.h :
#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

// Rewritten version of this hashtable: https://github.com/exebook/hashdict.c/tree/master

#ifndef HASHTABLE_DEF
#  define HASHTABLE_DEF static inline
#endif //HASHTABLE_DEF

#ifndef HASHTABLE_VALUE_TYPE
#  define HASHTABLE_VALUE_TYPE int
#endif //HASHTABLE_VALUE_TYPE

typedef enum{
  HASHTABLE_RET_ERROR = 0,
  HASHTABLE_RET_COLLISION,
  HASHTABLE_RET_SUCCESS,
}Hashtable_Ret;

typedef bool (*Hashtable_Func)(char *key, size_t key_len, HASHTABLE_VALUE_TYPE *value, size_t index, void *userdata);

typedef struct Hashtable_Key Hashtable_Key;

struct Hashtable_Key{
  Hashtable_Key *next;
  char *key;
  size_t len;
  HASHTABLE_VALUE_TYPE value;
};

HASHTABLE_DEF bool hashtable_key_init(Hashtable_Key **k, const char *key, size_t key_len);
HASHTABLE_DEF void hashtable_key_free(Hashtable_Key *k);

typedef struct{
  Hashtable_Key **table;
  size_t len;
  size_t count;
  double growth_treshold;
  double growth_factor;
  HASHTABLE_VALUE_TYPE *value;
}Hashtable;

HASHTABLE_DEF bool hashtable_init(Hashtable *ht, size_t initial_len);
HASHTABLE_DEF Hashtable_Ret hashtable_add(Hashtable *ht, const char *key, size_t key_len);
HASHTABLE_DEF bool hashtable_find(Hashtable *ht, const char *key, size_t key_len);
HASHTABLE_DEF bool hashtable_resize(Hashtable *ht, size_t new_len);
HASHTABLE_DEF void hashtable_reinsert_when_resizing(Hashtable *ht, Hashtable_Key *k2);
HASHTABLE_DEF void hashtable_for_each(Hashtable *ht, Hashtable_Func func, void *userdata);
HASHTABLE_DEF void hashtable_free(Hashtable *ht);

static inline uint32_t meiyan(const char *key, int count);

#ifdef HASHTABLE_IMPLEMENTATION

HASHTABLE_DEF bool hashtable_key_init(Hashtable_Key **_k, const char *key, size_t key_len) {

  Hashtable_Key *k = (Hashtable_Key *) malloc(sizeof(Hashtable_Key));
  if(!k) {
    return false;
  }
  k->len = key_len;
  k->key = (char *) malloc(sizeof(char) * k->len);
  if(!k->key) {
    return false;
  }
  memcpy(k->key, key, key_len);
  k->next = NULL;
  //k->value = -1; // TODO: check if it can be left empty

  *_k = k;
  
  return true;
}

HASHTABLE_DEF void hashtable_key_free(Hashtable_Key *k) {
  free(k->key);
  if(k->next) hashtable_key_free(k->next);
  free(k);
}

static inline uint32_t meiyan(const char *key, int count) {
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

HASHTABLE_DEF bool hashtable_init(Hashtable *ht, size_t initial_len) {

  if(initial_len == 0) {
    initial_len = 1024;
  }
  ht->len = initial_len;
  ht->count = 0;
  ht->table = malloc(sizeof(Hashtable_Key*) * ht->len);
  if(!ht->table) {
    return false;
  }
  memset(ht->table, 0, sizeof(Hashtable_Key*) * ht->len);
  ht->growth_treshold = 2.0;
  ht->growth_factor = 10.0;
  
  return true;
}

HASHTABLE_DEF Hashtable_Ret hashtable_add(Hashtable *ht, const char *key, size_t key_len) {
  int n = meiyan(key, (int) key_len) % ht->len;
  if(ht->table[n] == NULL) {
    double f = (double) ht->count / (double) ht->len;
    if(f > ht->growth_treshold ) {
      printf("HASHTABLE: resizing!\n"); fflush(stdout);
      if(!hashtable_resize(ht, (size_t) ((double) ht->len * ht->growth_factor) )) {
	return HASHTABLE_RET_ERROR;
      }
      return hashtable_add(ht, key, key_len);
    }
    if(!hashtable_key_init(&ht->table[n], key, key_len)) {
      return HASHTABLE_RET_ERROR;
    }
    ht->value = &ht->table[n]->value;
    ht->count++;
    return HASHTABLE_RET_SUCCESS;
  }
  Hashtable_Key *k = ht->table[n];
  while(k) {
    if(k->len == key_len && (memcmp(k->key, key, key_len) == 0) ) {
      ht->value = &k->value;
      return HASHTABLE_RET_COLLISION;
    }
    k = k->next;
  }
  ht->count++;
  Hashtable_Key *old = ht->table[n];
  if(!hashtable_key_init(&ht->table[n], key, key_len)) {
    return HASHTABLE_RET_ERROR;
  }
  ht->table[n]->next = old;
  ht->value = &ht->table[n]->value;

  printf("HASHTABLE: collision\n");
  return HASHTABLE_RET_SUCCESS;
}

HASHTABLE_DEF bool hashtable_find(Hashtable *ht, const char *key, size_t key_len) {
  int n = meiyan(key, (int) key_len) % ht->len;
#if defined(__MINGW32__) || defined(__MINGW64__)
  __builtin_prefetch(ht->table[n]);
#endif
    
#if defined(_WIN32) || defined(_WIN64)
  _mm_prefetch((char*)ht->table[n], _MM_HINT_T0);
#endif
  Hashtable_Key *k = ht->table[n];
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

HASHTABLE_DEF bool hashtable_resize(Hashtable *ht, size_t new_len) {

  size_t o = ht->len;
  Hashtable_Key **old = ht->table;
  ht->len = new_len;
  ht->table = malloc(sizeof(Hashtable_Key*) * ht->len);
  if(!ht->table) {
    return false;
  }
  memset(ht->table, 0, sizeof(Hashtable_Key*) * ht->len);
  for(size_t i=0;i<o;i++) {
    Hashtable_Key *k = old[i];
    while(k) {
      Hashtable_Key *next = k->next;
      k->next = NULL;
      hashtable_reinsert_when_resizing(ht, k);
      k = next;
    }
  }
  free(old);
  
  return true;
}

HASHTABLE_DEF void hashtable_reinsert_when_resizing(Hashtable *ht, Hashtable_Key *k2) {
  int n = meiyan(k2->key, (int) k2->len) % ht->len;
  if (ht->table[n] == NULL) {
    ht->table[n] = k2;
    ht->value = &ht->table[n]->value;
    return;
  }
  Hashtable_Key *k = ht->table[n];
  k2->next = k;
  ht->table[n] = k2;
  ht->value = &k2->value;
}

HASHTABLE_DEF void hashtable_for_each(Hashtable *ht, Hashtable_Func func, void *userdata) {
  size_t j = 0;
  for(size_t i=0;i<ht->len;i++) {
    if(ht->table[i] != NULL) {
      Hashtable_Key *k = ht->table[i];
      while(k) {
	if(!func(k->key, k->len, &k->value, j++, userdata)) {
	  return;
	}
	k = k->next;
      }
    }
  }
}

HASHTABLE_DEF void hashtable_free(Hashtable *ht) {
  for(size_t i=0;i<ht->len;i++) {
    if(ht->table[i]) {
      hashtable_key_free(ht->table[i]);
    }
  }
  free(ht->table);  
}

#endif //HASHTABLE_IMPLEMENTATION

#endif //HASHTABLE_H

typedef Hashtable *Json_Object;

typedef struct{
  size_t count;
  FILE *f;
}Json_Object_Data;

// json.h
#ifndef JSON_DEF
#  define JSON_DEF static inline
#endif //JSON_DEF

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

JSON_DEF bool json_array_init(Json_Array **_array);
JSON_DEF bool json_array_append(Json_Array *array, Json *json);
JSON_DEF void json_array_free(Json_Array *array);

JSON_DEF void json_fprint(FILE *f, Json json);

#define json_array_get(array, pos) *(Json *) ((unsigned char *) (array)->data + sizeof(Json) * (pos));
#define json_array_len(array) (array)->len
#define json_object_len(object) ((Hashtable *) (object))->count

#define json_null() (Json) {.kind = JSON_KIND_NULL }
#define json_false() (Json) {.kind = JSON_KIND_FALSE }
#define json_true() (Json) {.kind = JSON_KIND_TRUE }
#define json_number(d) (Json) {.kind = JSON_KIND_NUMBER, .as.doubleval = d }

#ifdef JSON_IMPLEMENTATION

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

  Hashtable **_ht = (Hashtable **) __ht;
  
  Hashtable *ht = malloc(sizeof(Hashtable));
  if(!ht) {
    return false;
  }

  if(!hashtable_init(ht, 0)) {
    return false;
  }

  *_ht = ht;

  return true;
}

JSON_DEF bool json_object_append(Json_Object_t *_ht, const char *key, Json *json) {

  Hashtable *ht = (Hashtable *) _ht;

  Hashtable_Ret ret = hashtable_add(ht, key, strlen(key));
  if(ret == HASHTABLE_RET_ERROR) {
    return false;
  }
  *ht->value = *json;
  
  return true;
}

JSON_DEF bool json_object_append2(Json_Object_t *_ht, const char *key, size_t key_len, Json *json) {
  Hashtable *ht = (Hashtable *) _ht;

  Hashtable_Ret ret = hashtable_add(ht, key, key_len);
  if(ret == HASHTABLE_RET_ERROR) {
    return false;
  }
  *ht->value = *json;
  
  return true;
  
}

JSON_DEF bool json_object_has(Json_Object_t *_ht, const char *key) {
  Hashtable *ht = (Hashtable *) _ht;
  return hashtable_find(ht, key, strlen(key));
}

JSON_DEF Json json_object_get(Json_Object_t *_ht, const char *key) {
  Hashtable *ht = (Hashtable *) _ht;
  hashtable_find(ht, key, strlen(key)); // no checking
  return *ht->value;
}

JSON_DEF void json_object_free(Json_Object_t *_ht) {
  hashtable_free((Hashtable *) _ht);
}

JSON_DEF bool json_string_init(char **string, const char *cstr) {
  size_t cstr_len = strlen(cstr) + 1;
  *string = malloc(cstr_len);
  if(!(*string)) {
    return false;
  }
  memcpy(*string, cstr, cstr_len);

  return true;
}

JSON_DEF bool json_string_init2(char **string, const char *cstr, size_t cstr_len) {
  *string = malloc(cstr_len + 1);
  if(!(*string)) {
    return false;
  }
  memcpy(*string, cstr, cstr_len);
  (*string)[cstr_len] = 0;
  return true;  
}

JSON_DEF void json_string_free(char *string) {
  free(string);
}
    
JSON_DEF bool json_array_init(Json_Array **_array) {
  
  Json_Array *array = (Json_Array *) malloc(sizeof(Json_Array));
  if(!array) {
    return false;
  }
    
  array->len  =  0;
  array->cap  = 16;
  array->data = malloc( array->cap * sizeof(Json) );
  if(!array->data) {
    return false;
  }

  *_array = array;

  return true;
}

JSON_DEF bool json_array_append(Json_Array *array, Json *json) {
  if( array->len >= array->cap ) {
    array->cap *= 2;
    array->data = realloc(array->data, array->cap * sizeof(Json));
    if(!array->data) {
      return false;
    }
  }
  void *ptr = (unsigned char *) array->data + sizeof(Json) * array->len++;
  memcpy(ptr, json, sizeof(Json));

  return true;
}

JSON_DEF void json_array_free(Json_Array *array) {
  free(array->data);
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
    Json_Object_Data data = { ((Hashtable *) json.as.objectval)->count, f};
    fprintf(f, "{");
    hashtable_for_each((Hashtable *) json.as.objectval, json_object_fprint, &data);
    fprintf(f, "}");
  } break;
  default: {
    fprintf(stderr, "ERROR: Unknown json kind: %s\n", json_kind_name(json.kind) );
    exit(1);
  }
  }
}

#endif //JSON_IMPLEMENTATION

#endif //JSON_H
