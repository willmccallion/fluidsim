#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define RES_X 1024
#define RES_Y 512

typedef struct {
  unsigned int id;
  int width, height;
} Texture2D_GL;

Texture2D_GL texDensity[2], texVelocity[2], texPressure[2], texDivergence,
    texObstacles;
unsigned int shdAdvect, shdDivergence, shdJacobi, shdSubtract;
Shader shdDisplay; // New Display Shader
int ping = 0;

Texture2D_GL CreateTexture2D(int w, int h, int format) {
  Texture2D_GL tex = {0, w, h};
  glGenTextures(1, &tex.id);
  glBindTexture(GL_TEXTURE_2D, tex.id);
  GLenum internalFormat = format;
  GLenum dataFormat = GL_RED;
  if (format == GL_RGBA16F)
    dataFormat = GL_RGBA;
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

void InitSim() {
  texDensity[0] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  texDensity[1] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  texVelocity[0] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  texVelocity[1] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  texPressure[0] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  texPressure[1] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  texDivergence = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  texObstacles = CreateTexture2D(RES_X, RES_Y, GL_R16F);

  // --- FIX: FASTER INITIAL SPEED ---
  float *vData = (float *)calloc(RES_X * RES_Y * 4, sizeof(float));
  for (int i = 0; i < RES_X * RES_Y; i++)
    vData[i * 4] = 400.0f; // Start fast

  glBindTexture(GL_TEXTURE_2D, texVelocity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  vData);
  glBindTexture(GL_TEXTURE_2D, texVelocity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  vData);
  free(vData);

  float *oData = (float *)calloc(RES_X * RES_Y, sizeof(float));
  for (int y = 0; y < RES_Y; y++) {
    for (int x = 0; x < RES_X; x++) {
      float cx = RES_X / 3.0f;
      float cy = RES_Y / 2.0f;
      bool solid = false;
      if (x > cx - 100 && x < cx && fabsf(y - cy) < 15 + (x - (cx - 100)) * 0.1)
        solid = true;
      if (x >= cx && x < cx + 120 && fabsf(y - cy) < 25)
        solid = true;
      if (x >= cx + 120 && x < cx + 140 && fabsf(y - cy) < 40)
        solid = true;
      if (x > cx - 60 && x < cx - 20 &&
          (fabsf(y - (cy - 50)) < 15 || fabsf(y - (cy + 50)) < 15))
        solid = true;
      if (x > cx + 80 && x < cx + 120 &&
          (fabsf(y - (cy - 50)) < 15 || fabsf(y - (cy + 50)) < 15))
        solid = true;
      if (solid)
        oData[y * RES_X + x] = 1.0f;
    }
  }
  glBindTexture(GL_TEXTURE_2D, texObstacles.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  oData);
  free(oData);

  shdAdvect =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_advect.glsl"));
  shdDivergence =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_div.glsl"));
  shdJacobi =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_jacobi.glsl"));
  shdSubtract = LoadCompute(LoadFileText("resources/shaders/fluid2d_sub.glsl"));

  // Load Display Shader
  shdDisplay = LoadShader(0, "resources/shaders/display.fs");
}

void UpdateSim(float dt, float time) {
  int p = ping;
  int next_p = !p;

  // 1. Advect
  rlEnableShader(shdAdvect);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texVelocity[p].id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texObstacles.id);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, texDensity[p].id);
  glBindImageTexture(3, texVelocity[next_p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(4, texDensity[next_p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlSetUniform(rlGetLocationUniform(shdAdvect, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(shdAdvect, "time"), &time,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlComputeShaderDispatch(RES_X / 16, RES_Y / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  ping = next_p;
  p = ping;

  // 2. Divergence
  rlEnableShader(shdDivergence);
  glBindImageTexture(0, texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(1, texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
  glBindImageTexture(2, texDivergence.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch(RES_X / 16, RES_Y / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 3. Pressure
  rlEnableShader(shdJacobi);
  for (int i = 0; i < 40; i++) {
    glBindImageTexture(0, texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, texDivergence.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(2, texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(3, texPressure[1].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_R16F);
    rlComputeShaderDispatch(RES_X / 16, RES_Y / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    Texture2D_GL tmp = texPressure[0];
    texPressure[0] = texPressure[1];
    texPressure[1] = tmp;
  }

  // 4. Subtract
  rlEnableShader(shdSubtract);
  glBindImageTexture(0, texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(1, texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA16F);
  glBindImageTexture(2, texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
  rlComputeShaderDispatch(RES_X / 16, RES_Y / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

int main() {
  InitWindow(1280, 640, "2D F1 Wind Tunnel");
  SetTargetFPS(60);
  InitSim();

  float time = 0.0f;
  while (!WindowShouldClose()) {
    float dt = 0.016f;
    time += dt;
    UpdateSim(dt, time);

    BeginDrawing();
    ClearBackground(BLACK);

    // --- FIX: DRAW WITH DISPLAY SHADER (GRAYSCALE) ---
    BeginShaderMode(shdDisplay);
    Texture2D raylibTex = {0};
    raylibTex.id = texDensity[ping].id;
    raylibTex.width = RES_X;
    raylibTex.height = RES_Y;
    raylibTex.format = PIXELFORMAT_UNCOMPRESSED_R32;
    raylibTex.mipmaps = 1;

    DrawTexturePro(raylibTex, (Rectangle){0, 0, (float)RES_X, (float)-RES_Y},
                   (Rectangle){0, 0, 1280, 640}, (Vector2){0, 0}, 0.0f, WHITE);
    EndShaderMode();

    // Draw Obstacles
    Texture2D obsTex = {0};
    obsTex.id = texObstacles.id;
    obsTex.width = RES_X;
    obsTex.height = RES_Y;
    obsTex.format = PIXELFORMAT_UNCOMPRESSED_R32;
    obsTex.mipmaps = 1;

    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(obsTex, (Rectangle){0, 0, (float)RES_X, (float)-RES_Y},
                   (Rectangle){0, 0, 1280, 640}, (Vector2){0, 0}, 0.0f, RED);
    EndBlendMode();

    DrawFPS(10, 10);
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
