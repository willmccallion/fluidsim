#include "renderer.h"
#include "raymath.h"
#include "rlgl.h"

#if defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

FluidRenderer InitFluidRenderer(void) {
  FluidRenderer r;
  r.shdRender = LoadShader(0, "resources/shaders/raymarch.fs");
  return r;
}

void DrawFluidSim(FluidRenderer *renderer, FluidSim *sim, Camera3D camera) {
  BeginMode3D(camera);

  // Cull Front faces so we see the Back faces of the volume box.
  rlEnableBackfaceCulling();
  rlSetCullFace(RL_CULL_FACE_FRONT);

  rlDisableDepthMask(); // Don't write to depth buffer

  BeginShaderMode(renderer->shdRender);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, sim->texDensity[sim->ping].id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, sim->texObstacles.id);

  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "texDensity"),
                 &(int){0}, SHADER_UNIFORM_INT);
  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "texObs"), &(int){1},
                 SHADER_UNIFORM_INT);

  Vector3 camForward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  Vector3 camRight =
      Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
  Vector3 camUp = Vector3CrossProduct(camRight, camForward);
  float resolution[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};

  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "camPos"),
                 &camera.position, SHADER_UNIFORM_VEC3);
  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "camDir"), &camForward,
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "camRight"), &camRight,
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "camUp"), &camUp,
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(renderer->shdRender,
                 GetShaderLocation(renderer->shdRender, "resolution"),
                 resolution, SHADER_UNIFORM_VEC2);

  // Draw the volume bounding box
  DrawCube((Vector3){RES_X / 2.0f, RES_Y / 2.0f, RES_Z / 2.0f}, RES_X, RES_Y,
           RES_Z, WHITE);

  EndShaderMode();

  // Reset state
  rlSetCullFace(RL_CULL_FACE_BACK);
  rlEnableDepthMask();

  // REMOVED: DrawCubeWires and DrawGrid to clean up the view

  EndMode3D();
}
