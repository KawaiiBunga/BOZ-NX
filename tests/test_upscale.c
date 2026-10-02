/* Exercise the actual scaler with a deterministic GLES/EGL implementation.
 * This checks ownership, dimensions, state restoration and failure cleanup;
 * Switch driver rendering still requires hardware testing. */
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "upscale.h"

static EGLContext context = (EGLContext)1;
static GLuint next_id = 100, fb = 17, rb = 19, tex[2] = {23, 29};
static GLenum active = GL_TEXTURE1;
static GLenum status = GL_FRAMEBUFFER_COMPLETE_OES;
static const char *extensions = "GL_OES_framebuffer_object GL_OES_draw_texture GL_OES_packed_depth_stencil";
static const char *missing;
static int allocations, deletes, storage_w, storage_h, draw_w, draw_h, draws;
static int filter_min, filter_mag, crop[4];

EGLContext eglGetCurrentContext(void) { return context; }
const GLubyte *glGetString(GLenum name) { assert(name == GL_EXTENSIONS); return (const GLubyte *)extensions; }
void glGetIntegerv(GLenum name, GLint *out) {
    switch (name) {
    case GL_FRAMEBUFFER_BINDING_OES: *out = fb; break;
    case GL_RENDERBUFFER_BINDING_OES: *out = rb; break;
    case GL_ACTIVE_TEXTURE: *out = active; break;
    case GL_TEXTURE_BINDING_2D: *out = tex[active - GL_TEXTURE0]; break;
    default: assert(0);
    }
}
void glActiveTexture(GLenum value) { active = value; }
void glGenTextures(GLsizei count, GLuint *out) { assert(count == 1); *out = ++next_id; ++allocations; }
void glBindTexture(GLenum target, GLuint value) { assert(target == GL_TEXTURE_2D); tex[active - GL_TEXTURE0] = value; }
void glTexParameteri(GLenum target, GLenum name, GLint value) {
    assert(target == GL_TEXTURE_2D);
    if (name == GL_TEXTURE_MIN_FILTER) filter_min = value;
    if (name == GL_TEXTURE_MAG_FILTER) filter_mag = value;
}
void glTexParameteriv(GLenum target, GLenum name, const GLint *value) {
    assert(target == GL_TEXTURE_2D && name == GL_TEXTURE_CROP_RECT_OES);
    memcpy(crop, value, sizeof crop);
}
void glTexImage2D(GLenum target, GLint level, GLint format, GLsizei w, GLsizei h,
                  GLint border, GLenum external, GLenum type, const void *data) {
    assert(target == GL_TEXTURE_2D && !level && !border && !data);
    assert(format == GL_RGBA && external == GL_RGBA && type == GL_UNSIGNED_BYTE);
    storage_w = w; storage_h = h;
}
void glDeleteTextures(GLsizei count, const GLuint *ids) { assert(count == 1 && *ids); ++deletes; }
static void gen(GLsizei count, GLuint *out) { glGenTextures(count, out); }
static void bind_fb(GLenum target, GLuint value) { assert(target == GL_FRAMEBUFFER_OES); fb = value; }
static void bind_rb(GLenum target, GLuint value) { assert(target == GL_RENDERBUFFER_OES); rb = value; }
static void rb_storage(GLenum target, GLenum format, GLsizei w, GLsizei h) {
    assert(target == GL_RENDERBUFFER_OES && format == GL_DEPTH24_STENCIL8_OES);
    assert(w == storage_w && h == storage_h);
}
static void fb_texture(GLenum target, GLenum attachment, GLenum texture_target, GLuint id, GLint level) {
    assert(target == GL_FRAMEBUFFER_OES && attachment == GL_COLOR_ATTACHMENT0_OES);
    assert(texture_target == GL_TEXTURE_2D && id && !level);
}
static void fb_renderbuffer(GLenum target, GLenum attachment, GLenum rb_target, GLuint id) {
    assert(target == GL_FRAMEBUFFER_OES && rb_target == GL_RENDERBUFFER_OES && id);
    assert(attachment == GL_DEPTH_ATTACHMENT_OES || attachment == GL_STENCIL_ATTACHMENT_OES);
}
static GLenum check(GLenum target) { assert(target == GL_FRAMEBUFFER_OES); return status; }
static void draw(GLint x, GLint y, GLint z, GLint w, GLint h) {
    assert(!x && !y && !z && fb == 0); draw_w = w; draw_h = h; ++draws;
}
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *name) {
    if (missing && !strcmp(name, missing)) return NULL;
#define PROC(n, f) if (!strcmp(name, #n)) return (__eglMustCastToProperFunctionPointerType)f;
    PROC(glGenRenderbuffersOES, gen)
    PROC(glBindRenderbufferOES, bind_rb)
    PROC(glRenderbufferStorageOES, rb_storage)
    PROC(glGenFramebuffersOES, gen)
    PROC(glBindFramebufferOES, bind_fb)
    PROC(glFramebufferTexture2DOES, fb_texture)
    PROC(glFramebufferRenderbufferOES, fb_renderbuffer)
    PROC(glCheckFramebufferStatusOES, check)
    PROC(glDeleteFramebuffersOES, glDeleteTextures)
    PROC(glDeleteRenderbuffersOES, glDeleteTextures)
    PROC(glDrawTexiOES, draw)
#undef PROC
    assert(0); return NULL;
}
void menu_blit_texture(unsigned texture, int w, int h) {
    assert(texture && fb == 0); upscale_draw_texture(w, h);
}
static void restored(void) { assert(fb == 17 && rb == 19 && active == GL_TEXTURE1 && tex[0] == 23 && tex[1] == 29); }
int main(void) {
    upscale_initialize(context, 720, 720);
    assert(!allocations && !upscale_active());
    missing = "glDrawTexiOES";
    upscale_initialize(context, 360, 720);
    assert(!allocations && !upscale_active()); restored();
    upscale_forget(context); missing = NULL;
    extensions = "GL_OES_framebuffer_object GL_OES_draw_texture";
    upscale_initialize(context, 540, 720);
    assert(!allocations && !upscale_active());
    upscale_forget(context);
    extensions = "GL_OES_framebuffer_object GL_OES_draw_texture GL_OES_packed_depth_stencil";
    status = GL_FRAMEBUFFER_UNSUPPORTED_OES;
    upscale_initialize(context, 540, 720);
    assert(allocations == 3 && deletes == 3 && !upscale_active()); restored();
    upscale_initialize(context, 540, 720); assert(allocations == 3);
    upscale_forget(context); status = GL_FRAMEBUFFER_COMPLETE_OES;
    upscale_initialize(context, 540, 720);
    assert(upscale_active() && storage_w == 960 && storage_h == 540);
    assert(filter_min == GL_LINEAR && filter_mag == GL_LINEAR);
    assert(crop[0] == 0 && crop[1] == 0 && crop[2] == 960 && crop[3] == 540); restored();
    assert(upscale_render_width(1280) == 960 && upscale_render_height(720) == 540);
    int before = allocations;
    upscale_initialize(context, 540, 720); assert(allocations == before);
    upscale_resume(); assert(fb > 100);
    upscale_present(1280, 720); assert(draws == 1 && draw_w == 1280 && draw_h == 720);
    upscale_resume(); GLuint owned = fb; assert(owned > 100);
    context = (EGLContext)2;
    assert(!upscale_active() && upscale_render_width(1280) == 1280);
    upscale_suspend(); upscale_present(1920, 1080); assert(fb == owned && draws == 1);
    upscale_forget(context); context = (EGLContext)1; assert(upscale_active());
    upscale_suspend(); assert(fb == 0);
    upscale_forget(context); assert(!upscale_active());
    upscale_initialize(context, 1080, 720);
    assert(storage_w == 1920 && storage_h == 1080 && upscale_active());
    upscale_present(1280, 720); assert(draws == 2 && draw_h == 720);
    upscale_forget(context);
    upscale_initialize(context, 1080, 1080); assert(!upscale_active());
    puts("PASS: scaler sizes, extension resolution, state restoration, cleanup, context ownership, presentation and fallback");
}
