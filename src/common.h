#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

typedef unsigned char u8;
typedef char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef unsigned long u64;
typedef long s64;

#define panic(...) do{ fprintf(stderr, "%s:%d: error: %s: ", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1);}while(0)

#endif //COMMON_H
