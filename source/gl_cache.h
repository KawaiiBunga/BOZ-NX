#ifndef BOZ_GL_CACHE_H
#define BOZ_GL_CACHE_H
#include <stdint.h>
void native_gl_cache_invalidate(void);
/* Returns 1 for a repeated binding; unknown GL targets always reach the driver. */
int native_gl_buffer_redundant(uint32_t target,uint32_t name);
uint32_t native_gl_direct(const char *name);
#endif
