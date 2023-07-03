#ifndef REGION_H
#define REGION_H

#include <stdlib.h>
#include <stdbool.h>

#ifndef REGION_DEF
#define REGION_DEF static inline
#endif //REGION_DEF

// TODO: implement aligned versions
/*
  void *aligned_malloc(size_t required_bytes, size_t alignment);
  void *aligned_realloc(void *p, size_t required_bytes, size_t alignment);
  void *aligned_free(void *p);
*/

typedef struct{
  uintptr_t *data;
  size_t capacity; // Capacity in words
  size_t length;   // Length   in words
}Region;

#define REGION_WORD_SIZE sizeof(uintptr_t)
#define region_cap(region) ( (region).capacity * REGION_WORD_SIZE )
#define region_len(region) ( (region).length * REGION_WORD_SIZE )
#define region_flush(region) (region).length = 0)
#define region_base(region) ( (region).data )

typedef struct{
  Region *region;
  size_t offset;  // Offset in words
}Region_Ptr;

#define region_ptr_null() (Region_Ptr) {.region = NULL, .offset = 0}
#define region_ptr_valid(ptr) ((ptr).region != NULL)

#define region_current(region) (Region_Ptr) { .region = (Region *) &(region), .offset = (region).length }
#define region_rewind(region, ptr) (region).length = (ptr).offset

REGION_DEF void *region_deref(Region_Ptr ptr);

REGION_DEF bool region_init(Region *region, size_t capacity_bytes);
REGION_DEF bool region_alloc(Region_Ptr *ptr, Region *region, size_t size_bytes);
REGION_DEF void region_free(Region *region);

/////////////////////////////////////////////////////////////////

// Linearly allocating Region
typedef struct{
  unsigned char *data;
  size_t capacity;   // Capacity in bytes
  size_t length;     // length   in bytes
}Region_Linear;

REGION_DEF bool region_linear_init(Region_Linear *linear, size_t capacity_bytes);
REGION_DEF bool region_linear_alloc(Region_Ptr *ptr, Region_Linear *region, size_t size_bytes);
REGION_DEF void region_linear_free(Region_Linear *region);

#ifdef REGION_IMPLEMENTATION

REGION_DEF bool region_linear_init(Region_Linear *region, size_t capacity_bytes) {

  unsigned char *data = malloc(capacity_bytes);
  if(!data) {
    return false;
  }

  region->data    =  data;
  region->length   = 0;
  region->capacity = capacity_bytes;
    
  return true;
}

REGION_DEF bool region_linear_alloc(Region_Ptr *ptr, Region_Linear *region, size_t size_bytes) {
    
  size_t needed_capacity = region->length + size_bytes;
  // Realloc words if necessary
  if(needed_capacity > region->capacity) {
    
    size_t new_capacity = region->capacity * 2;
    while( new_capacity < needed_capacity ) new_capacity *= 2;
    region->capacity = new_capacity;
    region->data = realloc(region->data, region->capacity);
    if(!region->data) {
      return false;
    }
    
  }

  ptr->region = (Region *) region;
  ptr->offset = region->length;
    
  region->length += size_bytes;

  return true;
  
}

REGION_DEF void region_linear_free(Region_Linear *region) {
  free(region->data);
}

#define region_deref(ptr) ( ((ptr).region)->data + (ptr).offset )
#define region_linear_deref(ptr) ( (unsigned char *) ((ptr).region)->data + (ptr).offset )

REGION_DEF bool region_init(Region *region, size_t capacity_bytes) {

  size_t capacity_words = (capacity_bytes + REGION_WORD_SIZE - 1) / REGION_WORD_SIZE;

  uintptr_t *data = malloc(capacity_words * REGION_WORD_SIZE);
  if(!data) {
    return false;
  }

  region->data    =  data;
  region->length   = 0;
  region->capacity = capacity_words;
    
  return true;
}

REGION_DEF bool region_alloc(Region_Ptr *ptr, Region *region, size_t size_bytes) {

  size_t size_words = (size_bytes + REGION_WORD_SIZE - 1) / REGION_WORD_SIZE;
    
  size_t needed_capacity = region->length + size_words;
  // Realloc words if necessary
  if(needed_capacity > region->capacity) {

    size_t new_capacity = region->capacity * 2;
    while( new_capacity < needed_capacity ) new_capacity *= 2;
    region->capacity = new_capacity;
    region->data = realloc(region->data, region->capacity * REGION_WORD_SIZE);
    if(!region->data) {
      return false;
    }
	
  }

  ptr->region = region;
  ptr->offset = region->length;
    
  region->length += size_words;

  return true;
}

REGION_DEF void region_free(Region *region) {
  free(region->data);
}

#endif // REGION_IMPLEMENTATION

#endif //REGION_H
