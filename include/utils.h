#ifndef UTILS_H
#define UTILS_H

typedef struct {
  unsigned int id;
  int width, height, depth;
} Texture3D;

Texture3D CreateTexture3D(int w, int h, int d, int internalFormat);
void Upload3DData(Texture3D tex, void *data, int format);
unsigned int LoadComputeShader(const char *fileName);

#endif
