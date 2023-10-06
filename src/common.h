#ifndef COMMON_H
#define COMMON_H

#define panic(...) do{ fprintf(stderr, "%s:%d: error: %s: ", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1);}while(0)

#endif //COMMON_H
