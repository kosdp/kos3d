/* shader.h - GLSL program sources, compile helpers, light uniform plumbing */
#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include "math3d.h"

/* program sources (defined in shader.c) */
extern const char *WORLD_VS, *WORLD_FS;
extern const char *LIT_VS,   *LIT_FS;
extern const char *EMIT_VS,  *EMIT_FS;
extern const char *POINT_VS, *POINT_FS;
extern const char *HUD_VS,   *HUD_FS;

GLuint compile(GLenum type, const char *src);
GLuint make_program(const char *vs, const char *fs);

/* cached light-array uniform locations for a lit program */
typedef struct { GLint np, lp, lc, lr, view; } LightU;
LightU light_locs(GLuint p);
void   upload_lights(LightU u, int nl, v3 *lp, v3 *lc, float *lr, v3 eye);

#endif /* SHADER_H */
