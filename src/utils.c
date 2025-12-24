#include "utils.h"
#include <stddef.h>

Texture2D_GL CreateTexture2D(int w, int h, int format) {
  Texture2D_GL tex = {0, w, h};
  glGenTextures(1, &tex.id);
  glBindTexture(GL_TEXTURE_2D, tex.id);

  GLenum internalFormat = format;
  GLenum dataFormat = GL_RGBA;
  if (format == GL_R16F)
    dataFormat = GL_RED;

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, GL_FLOAT,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

unsigned int LoadCompute(const char *code) {
  unsigned int shader = rlCompileShader(code, RL_COMPUTE_SHADER);
  unsigned int program = rlLoadComputeShaderProgram(shader);
  return program;
}
