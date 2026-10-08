#include "headless.h"
#include "utils.h"
#include <EGL/eglext.h>
#include <stddef.h>

#define MAX_EGL_DEVICES 16

static bool CreateContextOnDisplay(EGLDisplay display, EGLContext *outContext) {
  const EGLint configAttribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                  EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                  EGL_NONE};
  EGLConfig config;
  EGLint numConfigs = 0;
  if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) ||
      numConfigs == 0)
    return false;

  if (!eglBindAPI(EGL_OPENGL_API))
    return false;

  const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 4,
                                   EGL_CONTEXT_MINOR_VERSION, 3,
                                   EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                   EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                   EGL_NONE};
  EGLContext context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
  if (context == EGL_NO_CONTEXT)
    return false;

  if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
    eglDestroyContext(display, context);
    return false;
  }

  *outContext = context;
  return true;
}

static bool TryDevice(EGLDeviceEXT device, HeadlessContext *ctx) {
  PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (getPlatformDisplay == NULL)
    return false;

  EGLDisplay display =
      getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device, NULL);
  if (display == EGL_NO_DISPLAY)
    return false;

  EGLint major = 0, minor = 0;
  if (!eglInitialize(display, &major, &minor))
    return false;

  EGLContext context;
  if (!CreateContextOnDisplay(display, &context)) {
    eglTerminate(display);
    return false;
  }

  ctx->display = display;
  ctx->context = context;
  return true;
}

bool InitHeadlessContext(HeadlessContext *ctx) {
  PFNEGLQUERYDEVICESEXTPROC queryDevices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  if (queryDevices == NULL) {
    TraceLog(LOG_ERROR, "EGL: eglQueryDevicesEXT not available");
    return false;
  }

  EGLDeviceEXT devices[MAX_EGL_DEVICES];
  EGLint numDevices = 0;
  if (!queryDevices(MAX_EGL_DEVICES, devices, &numDevices) ||
      numDevices == 0) {
    TraceLog(LOG_ERROR, "EGL: No devices found");
    return false;
  }

  for (EGLint i = 0; i < numDevices; i++) {
    if (!TryDevice(devices[i], ctx))
      continue;
    rlLoadExtensions((void *)eglGetProcAddress);
    TraceLog(LOG_INFO, "EGL: Using device %d of %d: %s", i, numDevices,
             (const char *)glGetString(GL_RENDERER));
    return true;
  }

  TraceLog(LOG_ERROR, "EGL: No device could create a GL 4.3 core context");
  return false;
}

void CloseHeadlessContext(HeadlessContext *ctx) {
  eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(ctx->display, ctx->context);
  eglTerminate(ctx->display);
}
