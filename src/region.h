#ifndef REGION_H
#define REGION_H

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef char s8; // because of mingw warning not 'int8_t'
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

typedef float f32;
typedef double f64;

#endif // TYPES_H

#if !defined(REGION_PADDED) && !defined(REGION_LINEAR)
#  define REGION_LINEAR
#endif

#if !defined(REGION_DYNAMIC) && !defined(REGION_STATIC)
#  define REGION_DYNAMIC
#endif

#ifndef REGION_DEF
#  define REGION_DEF static inline
#endif //REGION_DEF

#ifdef REGION_VERBOSE
#  include <stdio.h>
#  define REGION_LOG(...) do{ fflush(stdout); fprintf(stderr, "REGION: " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while(0)
#else
#  define REGION_LOG(...)
#endif //REGION_VERBOSE

#define REGION_WORD_SIZE sizeof(uintptr_t)

typedef struct{
#if   defined(REGION_PADDED)
  uintptr_t *data;
#elif defined(REGION_LINEAR)
  u8 *data;
#endif                 //     REGION_PADDED/REGION_LINEAR
  u64 capacity;   // Capacity in words/bytes 
  u64 length;     // Length   in words/bytes

#ifdef REGION_DIAGNOSTICS
  u64 max_length;
#endif //REGION_DIAGNOSTICS
}Region;

typedef struct{
  Region  *region;  //           REGION_PADDED/REGION_LINEAR
  u64 offset;  // Offset in bytes
}Region_Ptr;

// Public
REGION_DEF bool region_init(Region *region, u64 capacity_bytes);
REGION_DEF bool region_alloc(Region_Ptr *ptr, Region *region, u64 size_bytes);
REGION_DEF void region_free(Region *region);

// 'Constant' Ptrs
void *region_base(Region region);
Region_Ptr region_current(Region *region);

void region_flush(Region *region);
void region_rewind(Region *region, Region_Ptr ptr);

void *region_deref(Region_Ptr ptr);

////////////////////////////////////////////////////////////////

#ifndef REGION_NO_STRING

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct{
  Region_Ptr data;
  size_t len;    
}string;

#define str_fmt "%.*s"
#define str_arg(s) (int) (s).len, (char *) region_deref((s).data)

typedef bool (*string_map_func)(const char *input, u64 input_size, char *buffer, u64 buffer_size, u64 *output_size);

// Public
REGION_DEF bool string_alloc(string *s, Region *region, const char *cstr);
REGION_DEF bool string_alloc2(string *s, Region *region, const char *cstr, u64 cstr_len);
REGION_DEF bool string_cstr_alloc(const char **cstr, Region *region, string s);
REGION_DEF bool string_copy(string *s, Region *region, string t);
REGION_DEF bool string_snprintf(string *s, Region *region, const char *fmt, ...);
REGION_DEF bool string_map(string *s, Region *region, string input, string_map_func map_func);
REGION_DEF bool string_map_cstr(string *s, Region *region, const char *cstr, string_map_func map_func);
REGION_DEF bool string_map_impl(string *s, Region *regon, const char *cstr, u64 cstr_len, string_map_func map_fun);

REGION_DEF s32 string_index_of(string s, const char *needle);
REGION_DEF s32 string_last_index_of(string s, const char *needle);
REGION_DEF s32 string_index_of_off(string s, u64 off, const char *needle);
REGION_DEF bool string_substring(string s, u64 start, u64 len, string *d);

REGION_DEF bool string_chop_by(string *s, const char *delim, string *d);

REGION_DEF bool string_eq(string s, string t);
REGION_DEF bool string_eq_cstr(string s, const char* cstr);
REGION_DEF bool string_eq_cstr2(string s, const char *cstr, u64 cstr_len);

REGION_DEF bool string_parse_s64(string s, s64 *n);
REGION_DEF bool string_parse_f64(string s, f64 *n);

//MACROS
const char *string_to_cstr(string s);
string string_from_cstr(const char* cstr);
string string_from_cstr2(const char* cstr, u64 cstr_len);
bool str_empty(string s);

#endif //REGION_NO_STRING

#ifdef REGION_IMPLEMENTATION

#define region_base(region) ( (region)->data )
#define region_deref(ptr) ( (*(unsigned char **) ((ptr).region) ) + (ptr).offset )
#define region_rewind(region, ptr) (region)->length = (ptr).offset
#define region_flush(region) (region)->length = 0
#define region_current(r) (Region_Ptr) { .region = (r), .offset = (r)->length }

#ifdef REGION_LINEAR
#  define region_cap(r) ((r)->capacity)
#else#elif defined(REGION_PADDED)
#  define region_cap(r) ((r)->capacity * REGION_WORD_SIZE)
#endif

#ifdef REGION_LINEAR
#  ifdef REGION_DIAGNOSTICS
#    define region_len(r) ((r)->max_length)
#  else
#    define region_len(r) ((r)->length)
#  endif //REGION_DIAGNOSTICS
#else#elif defined(REGION_PADDED)
#  ifdef REGION_DIAGNOSTICS
#    define region_len(r) ((r)->max_length * REGION_WORD_SIZE)
#  else
#    define region_len(r) ((r)->length * REGION_WORD_SIZE)
#  endif //REGION_DIAGNOSTICS
#endif

REGION_DEF bool region_init(Region *region, uint64_t capacity_bytes) {

#ifdef REGION_DIAGNOSTICS
  region->max_length = 0;
#endif //REGION_DIAGNOSTICS
  
#if   defined(REGION_PADDED)
    
  u64 capacity_words = (capacity_bytes + REGION_WORD_SIZE - 1) / REGION_WORD_SIZE;

  uintptr_t *data = (uintptr_t *) malloc(capacity_words * REGION_WORD_SIZE);
  if(!data) {
    REGION_LOG("Failed to initialize region");
    return false;
  }

  region->data    =  data;
  region->length   = 0;
  region->capacity = capacity_words;
    
  return true;
    
#elif defined(REGION_LINEAR)
    
  u8 *data = (u8 *) malloc(capacity_bytes);
  if(!data) {
    REGION_LOG("Failed to initialize region");
    return false;
  }

  region->data    =  data;
  region->length   = 0;
  region->capacity = capacity_bytes;
    
  return true;
#else
    
  (void) region;
  (void) capacity_bytes;
  return false;
    
#endif    
}

REGION_DEF bool region_alloc(Region_Ptr *ptr, Region *region, u64 size_bytes) {
#if   defined(REGION_PADDED)
  u64 size_words = (size_bytes + REGION_WORD_SIZE - 1) / REGION_WORD_SIZE;
    
  u64 needed_capacity = region->length + size_words;
  // Realloc words if necessary
  if(needed_capacity > region->capacity) {
#if   defined(REGION_DYNAMIC)
    u64 new_capacity = region->capacity * 2;
    while( new_capacity < needed_capacity ) new_capacity *= 2;

#ifdef REGION_VERBOSE

    void *old_data = region->data;
    u64 old_capacity = region->capacity;

#endif REGION_VERBOSE
    
    region->capacity = new_capacity;
    region->data = realloc(region->data, region->capacity * REGION_WORD_SIZE);

    REGION_LOG("Reallocating from %zu to %zu bytes.\n"
	       "    data: %p -> %p",
	       (old_capacity * REGION_WORD_SIZE), (region->capacity * REGION_WORD_SIZE),
	       old_data, (void *) region->data);
    
    if(!region->data) {
      
      REGION_LOG("Failed to allocate %zu bytes. Failed to reallocate memory.\n"
		 "    capacity: %zu.\n"
		 "    length:   %zu.\n"
		 "    configuration: DYNAMIC | PADDED",
		 size_bytes, region->capacity, region->length);
      return false;
    }
#elif defined(REGION_STATIC)
    
    REGION_LOG("Failed to allocate %zu bytes. Exceeded capacity.\n"
	       "    capacity: %zu.\n"
	       "    length:   %zu.\n"
	       "    configuration: STATIC | PADDED",
	       size_bytes, region->capacity, region->length);
    return false;
#else
    REGION_LOG("This point should be unreachable. Only if you explicity #undef'ined the default region configuration");x
    return false;
#endif
  }

  ptr->region = region;
  ptr->offset = sizeof(*region->data) * region->length;
    
  region->length += size_words;

#ifdef REGION_DIAGNOSTICS
  if(region->length > region->max_length) region->max_length = region->length;
#endif //REGION_DIAGNOSTICS

  return true;
    
#elif defined(REGION_LINEAR)

  u64 needed_capacity = region->length + size_bytes;
  // Realloc words if necessary
  if(needed_capacity > region->capacity) {
#if   defined(REGION_DYNAMIC)
    u64 new_capacity = region->capacity * 2;
    while( new_capacity < needed_capacity ) new_capacity *= 2;

#ifdef REGION_VERBOSE

    void *old_data = region->data;
    u64 old_capacity = region->capacity;

#endif REGION_VERBOSE
        
    region->capacity = new_capacity;
    region->data = realloc(region->data, region->capacity);

    REGION_LOG("Reallocating from %zu to %zu bytes.\n"
	       "    data: %p -> %p",
	       old_capacity, region->capacity,
	       old_data, (void *) region->data);
    
    if(!region->data) {
      REGION_LOG("Failed to allocate %zu bytes. Failed to reallocate memory.\n"
		 "    capacity: %zu.\n"
		 "    length:   %zu.\n"
		 "    configuration: DYNAMIC | LINEAR",
		 size_bytes, region->capacity, region->length);
      return false;
    }
#elif defined(REGION_STATIC)
    REGION_LOG("Failed to allocate %zu bytes. Exceeded capacity.\n"
	       "    capacity: %zu.\n"
	       "    length:   %zu.\n"
	       "    configuration: STATIC | LINEAR",
	       size_bytes, region->capacity, region->length);
    return false;
#else
    REGION_LOG("This point should be unreachable. Only if you explicity #undef'ined the default region configuration");x
    return false;
#endif
  }

  ptr->region = region;
  ptr->offset = region->length;
    
  region->length += size_bytes;

#ifdef REGION_DIAGNOSTICS
  if(region->length > region->max_length) region->max_length = region->length;
#endif //REGION_DIAGNOSTICS

  return true;
    
#else
  REGION_LOG("This point should be unreachable. Only if you explicitly #undef'ined the default region configuration");

  (void) ptr;
  (void) region;
  (void) size_bytes;
  return false;
    
#endif
}

REGION_DEF void region_free(Region *region) {
  free(region->data);
}

//////////////////////////////////////////////////////////

#ifndef REGION_NO_STRING

#define string_from_cstr(cstr) (string) { (Region_Ptr) { (Region *) &(cstr), 0}, strlen((cstr)) }
#define string_from_cstr2(cstr, cstr_len) (string) { (Region_Ptr) { (Region *) &(cstr), 0}, (cstr_len) }
#define string_to_cstr(s) ((char *) (memcpy(memset(_alloca(s.len + 1), 0, s.len + 1), region_deref(s.data), s.len)) )
#define string_substring_impl(s, start, len) (string) { .data = (Region_Ptr) { .region = s.data.region, .offset = s.data.offset + start}, .len = len}
#define str_empty(s) ((s).len == 0)

REGION_DEF bool string_alloc(string *s, Region *region, const char *cstr) {
  s->len = strlen(cstr);
  if(!region_alloc(&s->data, region, s->len)) return false;
  memcpy( region_deref(s->data), cstr, s->len);
  return true;
}

REGION_DEF bool string_snprintf(string *s, Region *region, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
#ifdef linux
  va_list args_copy;
  va_copy(args_copy, args);
#else
  va_list args_copy = args;
#endif
  s->len = vsnprintf(NULL, 0, fmt, args) + 1;
  va_end(args);
  if(!region_alloc(&s->data, region, s->len)) return false;
  vsnprintf( (char *const) region_deref(s->data), s->len, fmt, args_copy);
  if(s->len) s->len--;
  return true;
}

REGION_DEF bool string_map_impl(string *s, Region *region,
				const char *cstr, u64 cstr_len,
				string_map_func map_func) {
  s->len = cstr_len;
  if(!region_alloc(&s->data, region, s->len)) return false;
  u64 output_size;
  bool map_success = map_func( cstr, cstr_len, (char *) region_deref(s->data), s->len, &output_size);
  while(!map_success) {
    s->len *= 2;
    region_rewind(region, s->data);
    if(!region_alloc(&s->data, region, s->len)) return false;
    map_success = map_func( cstr, cstr_len, (char *) region_deref(s->data), s->len, &output_size);
  }  
  s->len = output_size;
  region_rewind(region, s->data);
  if(!region_alloc(&s->data, region, s->len)) return false;
  return true;  
}

static int string_index_of_impl(const char *haystack, u64 haystack_size, const char* needle, u64 needle_size) {
  if(needle_size > haystack_size) {
    return -1;
  }
  haystack_size -= needle_size;
  u64 i, j;
  for(i=0;i<=haystack_size;i++) {
    for(j=0;j<needle_size;j++) {
      if(haystack[i+j] != needle[j]) {
	break;
      }
    }
    if(j == needle_size) {
      return (int) i;
    }
  }
  return -1;

}

REGION_DEF int string_index_of(string s, const char *needle) {

  const char *haystack = (const char *) region_deref(s.data);
  u64 haystack_size = s.len;

  return string_index_of_impl(haystack, haystack_size, needle, strlen(needle));
}

REGION_DEF int string_index_of_off(string s, u64 off, const char *needle) {

  if(off > s.len) {
    return -1;
  }

  const char *haystack = (const char *) region_deref(s.data);
  u64 haystack_size = s.len;
  
  s32 pos = string_index_of_impl(haystack + off, haystack_size - off, needle, strlen(needle));
  if(pos < 0) {
    return -1;
  }

  return pos + (s32) off;
}

static s32 string_last_index_of_impl(const char *haystack, u64 haystack_size, const char* needle, u64 needle_size) {

  if(needle_size > haystack_size) {
    return -1;
  }
  
  u64 i;

  for(i=haystack_size - needle_size - 1;i>=0;i--) {
    u64 j;
    for(j=0;j<needle_size;j++) {
      if(haystack[i+j] != needle[j]) {
	break;
      }
    }
    if(j == needle_size) {
      return (s32) i;
    }
  }
  
  return -1;
}

REGION_DEF s32 string_last_index_of(string s, const char *needle) {
  const char *haystack = (const char *) region_deref(s.data);
  u64 haystack_size = s.len;

  return string_last_index_of_impl(haystack, haystack_size, needle, strlen(needle));
}

REGION_DEF bool string_substring(string s, u64 start, u64 len, string *d) {

  if(start > s.len) {
    return false;
  }

  if(start + len > s.len) {
    return false;
  }

  *d = string_substring_impl(s, start, len);
  
  return true;
}

REGION_DEF bool string_chop_by(string *s, const char *delim, string *d) {
  if(!s->len) return false;
  
  s32 pos = string_index_of(*s, delim);
  if(pos < 0) pos = (int) s->len;
    
  if(d && !string_substring(*s, 0, pos, d))
    return false;

  if(pos == (int) s->len) {
    s->len = 0;
    return false;
  } else {
    return string_substring(*s, pos + 1, s->len - pos - 1, s);
  }

}

REGION_DEF bool string_map(string *s, Region *region, string input, string_map_func map_func) {
  return string_map_impl(s, region, (const char *) region_deref(input.data), input.len, map_func);
}

REGION_DEF bool string_map_cstr(string *s, Region *region, const char *cstr, string_map_func map_func) {
  return string_map_impl(s, region, cstr, strlen(cstr), map_func); 
}

REGION_DEF bool string_alloc2(string *s, Region *region, const char *cstr, u64 cstr_len) {
  s->len = cstr_len;
  if(!region_alloc(&s->data, region, s->len)) return false;
  memcpy( region_deref(s->data), cstr, s->len);
  return true;
}

REGION_DEF bool string_cstr_alloc(const char **cstr, Region *region, string s) {
  Region_Ptr ptr;
  if(!region_alloc(&ptr, region, s.len + 1)) return false;
  u8 *data = region_deref(ptr);
  memcpy( data, region_deref(s.data), s.len);
  data[s.len] = 0;
  *cstr = (const char *) data;
  return true;
}

REGION_DEF bool string_copy(string *s, Region *region, string t) {
  s->len = t.len;
  if(!region_alloc(&s->data, region, s->len)) return false;
  memcpy( region_deref(s->data), region_deref(t.data), s->len);
  return true;  
}

REGION_DEF bool string_eq_cstr(string s, const char* cstr) {
  return string_eq_cstr2(s, cstr, strlen(cstr));
}

REGION_DEF bool string_eq_cstr2(string s, const char *cstr, size_t cstr_len) {
  if(s.len != cstr_len) return false;
  return memcmp( region_deref(s.data), cstr, cstr_len) == 0;  
}

REGION_DEF bool string_eq(string s, string t) {
  if(s.len != t.len) return false;
  return memcmp( region_deref(s.data), region_deref(t.data), s.len) == 0;
}

REGION_DEF bool string_parse_s64(string s, s64 *n) {
  u64 i=0;
  s64 sum = 0;
  int negative = 0;

  char *data = (char *) region_deref(s.data);
  
  if(s.len && data[0]=='-') {
    negative = 1;
    i++;
  }  
  while(i<s.len &&
	'0' <= data[i] && data[i] <= '9') {
    sum *= 10;
    s32 digit = data[i] - '0';
    sum += digit;
    i++;
  }

  if(negative) sum*=-1;
  *n = sum;

  return i>0;
}

REGION_DEF bool string_parse_f64(string s, f64 *n) {
  if (s.len == 0) {
    return false;
  }

  double parsedResult = 0.0;
  int sign = 1;
  int decimalFound = 0;
  int decimalPlaces = 0;
  double exponentFactor = 1.0;

  u8 *data = region_deref(s.data);

  size_t i = 0;

  // Check for a sign character
  if (i < s.len && (data[i] == '+' || data[i] == '-')) {
    if (data[i] == '-') {
      sign = -1;
    }
    i++;
  }

  // Parse the integral part of the number
  while (i < s.len && isdigit(data[i])) {
    parsedResult = parsedResult * 10.0 + (data[i] - '0');
    i++;
  }

  // Parse the fractional part of the number
  if (i < s.len && data[i] == '.') {
    i++;
    while (i < s.len && isdigit(data[i])) {
      parsedResult = parsedResult * 10.0 + (data[i] - '0');
      decimalPlaces++;
      i++;
    }
    decimalFound = 1;
  }

  exponentFactor = 1.0;
  for (int j = 0; j < decimalPlaces; j++) {
    exponentFactor *= 10.0;
  }
  
  // Adjust the result based on sign, decimal places, and exponent
  parsedResult *= sign;
  if (decimalFound) {
    parsedResult /= exponentFactor;
  }

  // Set the parsed result via the pointer argument
  *n = parsedResult;

  return true; // Parsing was successful
}

#endif //REGION_NO_STRING

#endif // REGION_IMPLEMENTATION


#endif //REGION_H
