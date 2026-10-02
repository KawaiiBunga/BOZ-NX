/* GLES1 bilinear presentation: one scene render, one hardware texture draw.
 * Owned objects never enter the game's texture/VBO tracking. */
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <stdio.h>
#include <string.h>
#include "upscale.h"
#include "display_options.h"
extern void menu_blit_texture(unsigned texture, int width, int height);
static void *owner;
static GLuint framebuffer, texture, depth;
static int width, height, attempted;
#define OES_FUNCTIONS(X) \
    X(PFNGLGENRENDERBUFFERSOESPROC, glGenRenderbuffersOES) \
    X(PFNGLBINDRENDERBUFFEROESPROC, glBindRenderbufferOES) \
    X(PFNGLRENDERBUFFERSTORAGEOESPROC, glRenderbufferStorageOES) \
    X(PFNGLGENFRAMEBUFFERSOESPROC, glGenFramebuffersOES) \
    X(PFNGLBINDFRAMEBUFFEROESPROC, glBindFramebufferOES) \
    X(PFNGLFRAMEBUFFERTEXTURE2DOESPROC, glFramebufferTexture2DOES) \
    X(PFNGLFRAMEBUFFERRENDERBUFFEROESPROC, glFramebufferRenderbufferOES) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSOESPROC, glCheckFramebufferStatusOES) \
    X(PFNGLDELETEFRAMEBUFFERSOESPROC, glDeleteFramebuffersOES) \
    X(PFNGLDELETERENDERBUFFERSOESPROC, glDeleteRenderbuffersOES) \
    X(PFNGLDRAWTEXIOESPROC, glDrawTexiOES)
#define DECLARE(type, name) static type name;
OES_FUNCTIONS(DECLARE)
#undef DECLARE

static int resolve_extensions(void) {
    /* Mesa exposes OES entry points through EGL, rather than static symbols. */
#define RESOLVE(type, name) name = (type)eglGetProcAddress(#name); if (!name) return 0;
    OES_FUNCTIONS(RESOLVE)
#undef RESOLVE
    return 1;
}

static int extension(const char *list, const char *name) {
    if (!list) return 0;
    size_t length = strlen(name);
    for (const char *p = list; (p = strstr(p, name)); p += length)
        if ((p == list || p[-1] == ' ') && (p[length] == 0 || p[length] == ' ')) return 1;
    return 0;
}
void upscale_forget(void *context) {
    if (context != owner) return;
    /* EGL releases these with their context; no GL calls after destruction. */
    owner = NULL; framebuffer = texture = depth = 0; attempted = 0;
}
void upscale_initialize(void *context, int render_height, int output_height) {
    if (render_height_valid(render_height) == output_height) return;
    if (owner != context) {
        owner = context; framebuffer = texture = depth = 0; attempted = 0;
    }
    if (attempted) return;
    attempted = 1;
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (!extension(extensions, "GL_OES_framebuffer_object") ||
        !extension(extensions, "GL_OES_draw_texture") ||
        !extension(extensions, "GL_OES_packed_depth_stencil") || !resolve_extensions()) {
        printf("  [upscale] required GLES1 extensions unavailable; direct output\n"); return;
    }
    GLint old_framebuffer = 0, old_depth = 0, old_active = 0, old_texture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, &old_framebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING_OES, &old_depth);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    height = render_height_valid(render_height); width = render_width_for_height(height);
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GLint crop[4] = {0, 0, width, height};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);
    glGenRenderbuffersOES(1, &depth); glBindRenderbufferOES(GL_RENDERBUFFER_OES, depth);
    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH24_STENCIL8_OES, width, height);
    glGenFramebuffersOES(1, &framebuffer); glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
    glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, texture, 0);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depth);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_STENCIL_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depth);
    GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, (GLuint)old_framebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, (GLuint)old_depth);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture); glActiveTexture((GLenum)old_active);
    if (status != GL_FRAMEBUFFER_COMPLETE_OES || !framebuffer || !texture || !depth) {
        printf("  [upscale] incomplete framebuffer %x; direct output\n", status);
        glDeleteFramebuffersOES(1, &framebuffer); glDeleteRenderbuffersOES(1, &depth); glDeleteTextures(1, &texture);
        framebuffer = texture = depth = 0;
        return;
    }
    printf("  [upscale] %dx%d -> %dx%d, bilinear texture presentation\n", width, height,
           output_height * 16 / 9, output_height);
}
int upscale_active(void) { return framebuffer && eglGetCurrentContext() == owner; }
int upscale_render_width(int fallback) { return upscale_active() ? width : fallback; }
int upscale_render_height(int fallback) { return upscale_active() ? height : fallback; }
void upscale_present(int output_width, int output_height) {
    if (!upscale_active()) return;
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    menu_blit_texture(texture, output_width, output_height);
}
void upscale_resume(void) { if (upscale_active()) glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer); }
void upscale_suspend(void) { if (upscale_active()) glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0); }
void upscale_draw_texture(int output_width, int output_height) {
    if (upscale_active()) glDrawTexiOES(0, 0, 0, output_width, output_height);
}
