#include "raylib.h"
#include "rlgl.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

// Linux GL Headers
#if defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

Texture3D CreateTexture3D(int w, int h, int d, int internalFormat) {
  Texture3D tex = {0, w, h, d};
  glGenTextures(1, &tex.id);
  glBindTexture(GL_TEXTURE_3D, tex.id);

  GLenum format = GL_RED;
  if (internalFormat == GL_RGBA16F)
    format = GL_RGBA;

  glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, w, h, d, 0, format, GL_FLOAT,
               NULL);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  return tex;
}

void Upload3DData(Texture3D tex, void *data, int format) {
  glBindTexture(GL_TEXTURE_3D, tex.id);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, tex.width, tex.height, tex.depth,
                  format, GL_FLOAT, data);
}

unsigned int LoadComputeShader(const char *fileName) {
  char *code = LoadFileText(fileName);
  unsigned int shader = rlCompileShader(code, RL_COMPUTE_SHADER);
  if (shader == 0) {
    printf("CRITICAL: Failed to compile shader: %s\n", fileName);
  }
  unsigned int program = rlLoadComputeShaderProgram(shader);
  if (program == 0) {
    printf("CRITICAL: Failed to link shader program: %s\n", fileName);
  }
  UnloadFileText(code);
  return program;
}
