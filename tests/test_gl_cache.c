#include <assert.h>
#include <stdio.h>
#include "gl_cache.h"
int main(void) {
  native_gl_cache_invalidate();
  assert(!native_gl_buffer_redundant(0x8892,0));
  assert(native_gl_buffer_redundant(0x8892,0));
  assert(!native_gl_buffer_redundant(0x8893,0));
  assert(!native_gl_buffer_redundant(0x8892,7));
  assert(native_gl_buffer_redundant(0x8893,0));
  assert(!native_gl_buffer_redundant(0xffff,7));
  assert(!native_gl_buffer_redundant(0xffff,7)); /* Preserve invalid-target error path */
  native_gl_cache_invalidate(); /* context switch, overlay, or deletion */
  assert(!native_gl_buffer_redundant(0x8892,7));
  puts("PASS: GL bindings are cached per target and invalidated for external changes");
}
