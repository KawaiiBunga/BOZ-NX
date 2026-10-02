#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t u8;
static inline void randomGet(void *buffer, size_t size) { memset(buffer, 0x42, size); }
