/* Direct GL entrypoints are legal because the game and Mesa now share 32-bit
 * pointers and softfp AAPCS. Only exact passthroughs are listed: functions
 * needing viewport, input, texture or draw tracking stay in the SDK layer. */
#include <GLES/gl.h>
#include <stdint.h>
#include <string.h>
#include "gl_cache.h"
static uint32_t buffer[2];
static unsigned known;
void native_gl_cache_invalidate(void) { known=0; }
int native_gl_buffer_redundant(uint32_t target,uint32_t name) {
  int index=target==0x8892u?0:target==0x8893u?1:-1;
  if(index<0)return 0;
  unsigned bit=1u<<index;
  if((known&bit)&&buffer[index]==name)return 1;
  buffer[index]=name; known|=bit; return 0;
}
uint32_t native_gl_direct(const char *name) {
#define DIRECT(fn) if (!strcmp(name,#fn)) return (uint32_t)(uintptr_t)&fn
  if(!name)return 0;
  DIRECT(glLoadIdentity); DIRECT(glLoadMatrixf); DIRECT(glLoadMatrixx);
  DIRECT(glMultMatrixf); DIRECT(glMultMatrixx); DIRECT(glPushMatrix); DIRECT(glPopMatrix);
  DIRECT(glRotatef); DIRECT(glRotatex); DIRECT(glScalef); DIRECT(glScalex);
  DIRECT(glTranslatef); DIRECT(glTranslatex); DIRECT(glColor4f); DIRECT(glColor4x);
  DIRECT(glNormal3f); DIRECT(glNormal3x); DIRECT(glClearColor); DIRECT(glClearColorx);
  DIRECT(glClearDepthf); DIRECT(glClearDepthx); DIRECT(glColorMask); DIRECT(glDepthFunc);
  DIRECT(glDepthMask); DIRECT(glAlphaFunc); DIRECT(glAlphaFuncx); DIRECT(glBlendFunc);
  DIRECT(glCullFace); DIRECT(glFrontFace); DIRECT(glHint);
  DIRECT(glFogf); DIRECT(glFogx); DIRECT(glFogfv); DIRECT(glFogxv);
  DIRECT(glLightf); DIRECT(glLightx); DIRECT(glLightfv); DIRECT(glLightxv);
  DIRECT(glMaterialf); DIRECT(glMaterialx); DIRECT(glMaterialfv); DIRECT(glMaterialxv);
  DIRECT(glTexEnvf); DIRECT(glTexEnvx); DIRECT(glTexEnvfv); DIRECT(glTexEnvxv);
  DIRECT(glTexParameterf); DIRECT(glTexParameterx); DIRECT(glTexParameterfv); DIRECT(glTexParameterxv);
  DIRECT(glGetError);
#undef DIRECT
  return 0;
}
