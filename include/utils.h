#ifndef UTILS_H
#define UTILS_H

#include "raylib.h"
#include "rlgl.h"

#if defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

typedef struct {
  unsigned int id;
  int width, height;
} Texture2D_GL;

Texture2D_GL CreateTexture2D(int w, int h, int format);
unsigned int LoadCompute(const char *code);

#endif
