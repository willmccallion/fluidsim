#ifndef HEADLESS_H
#define HEADLESS_H

#include <EGL/egl.h>
#include <stdbool.h>

/** Windowless OpenGL 4.3 core context created through EGL. */
typedef struct {
  EGLDisplay display;
  EGLContext context;
} HeadlessContext;

/**
 * Creates a surfaceless GL 4.3 context on the first EGL device that supports
 * one, makes it current and loads GL entry points for rlgl.
 */
bool InitHeadlessContext(HeadlessContext *ctx);
void CloseHeadlessContext(HeadlessContext *ctx);

#endif
