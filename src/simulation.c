#include "simulation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy

void InitSim(FluidSim *sim) {
  sim->ping = 0;
  sim->enableWindTunnel = true;
  sim->buoyancyStrength = 8.0f;
  sim->windSpeed = 1200.0f;

  // Initialize Smooth Stats
  sim->maxPressureSmooth = 1.0f;
  sim->maxVelocitySmooth = 1.0f;
  sim->maxCurlSmooth = 1.0f;

  // Create Textures
  sim->texDensity[0] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texDensity[1] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texVelocity[0] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texVelocity[1] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texPressure[0] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texPressure[1] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texDivergence = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texCurl = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texObstacles = CreateTexture2D(RES_X, RES_Y, GL_R16F);

  // Load Shaders
  sim->shdAdvect =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_advect.glsl"));
  sim->shdDivergence =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_div.glsl"));
  sim->shdJacobi =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_jacobi.glsl"));
  sim->shdSubtract =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_sub.glsl"));
  sim->shdCurl =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_curl.glsl"));
  sim->shdVorticity =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_vort.glsl"));
  sim->shdSplat =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_splat.glsl"));
  sim->shdInlet =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_inlet.glsl"));
  sim->shdPaint =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_paint.glsl"));

  // Analysis Shaders
  sim->shdForce =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_force.glsl"));
  sim->shdAnalyze =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_analyze.glsl"));

  // Init Stats SSBO
  glGenBuffers(1, &sim->ssboStats);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboStats);
  glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(unsigned int), NULL,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Init Force SSBO
  glGenBuffers(1, &sim->ssboForce);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
  glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(int), NULL,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  ResetSim(sim, 1);
}

void ResetSim(FluidSim *sim, int mode) {
  // Reset Smooth Stats
  sim->maxPressureSmooth = 0.1f;
  sim->maxVelocitySmooth = 0.1f;

  float *zeroData = (float *)calloc(RES_X * RES_Y * 4, sizeof(float));

  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texDensity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texDensity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texPressure[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texPressure[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);

  free(zeroData);

  if (mode == 1) {
    sim->buoyancyStrength = 0.0f;
    float *oData = (float *)calloc(RES_X * RES_Y, sizeof(float));
    for (int y = 0; y < RES_Y; y++) {
      for (int x = 0; x < RES_X; x++) {
        float cx = RES_X / 4.0f;
        float cy = RES_Y / 2.0f;
        float scale = RES_X / 1024.0f;
        bool solid = false;

        if (x > cx - (100 * scale) && x < cx &&
            fabsf(y - cy) < (15 * scale) + (x - (cx - (100 * scale))) * 0.1)
          solid = true;
        if (x >= cx && x < cx + (120 * scale) && fabsf(y - cy) < (25 * scale))
          solid = true;
        if (x >= cx + (120 * scale) && x < cx + (140 * scale) &&
            fabsf(y - cy) < (40 * scale))
          solid = true;
        if (x > cx - (60 * scale) && x < cx - (20 * scale) &&
            (fabsf(y - (cy - (50 * scale))) < (15 * scale) ||
             fabsf(y - (cy + (50 * scale))) < (15 * scale)))
          solid = true;
        if (x > cx + (80 * scale) && x < cx + (120 * scale) &&
            (fabsf(y - (cy - (50 * scale))) < (15 * scale) ||
             fabsf(y - (cy + (50 * scale))) < (15 * scale)))
          solid = true;

        if (solid)
          oData[y * RES_X + x] = 1.0f;
      }
    }
    glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                    oData);
    free(oData);
  } else {
    sim->buoyancyStrength = 8.0f;
  }
}

void ApplySplat(FluidSim *sim, Texture2D_GL tex, Vector2 pos, float radius,
                Vector4 color) {
  rlEnableShader(sim->shdSplat);
  glBindImageTexture(0, tex.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "color"), &color,
               RL_SHADER_UNIFORM_VEC4, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

void PaintObstacle(FluidSim *sim, Vector2 pos, float radius, bool erase) {
  rlEnableShader(sim->shdPaint);
  glBindImageTexture(0, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_R16F);
  float val = erase ? 0.0f : 1.0f;
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "value"), &val,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

void UpdateSim(FluidSim *sim, float dt, float time) {
  int p = sim->ping;
  int next_p = !p;
  Vector2 res = {(float)RES_X, (float)RES_Y};

  // 1. Advect
  rlEnableShader(sim->shdAdvect);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[p].id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[p].id);
  glBindImageTexture(3, sim->texVelocity[next_p].id, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, sim->texDensity[p].id);
  glBindImageTexture(3, sim->texDensity[next_p].id, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  sim->ping = next_p;
  p = sim->ping;

  // 2. Inlet
  if (sim->enableWindTunnel) {
    rlEnableShader(sim->shdInlet);
    glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    glBindImageTexture(1, sim->texDensity[p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "time"), &time,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "windSpeed"), &sim->windSpeed,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlComputeShaderDispatch(2, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    rlDisableShader();
  }

  // 3. Curl
  rlEnableShader(sim->shdCurl);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 4. Vorticity
  rlEnableShader(sim->shdVorticity);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
  float curlStr = 5.0f;
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "curlStrength"),
               &curlStr, RL_SHADER_UNIFORM_FLOAT, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 5. Divergence
  rlEnableShader(sim->shdDivergence);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(2, sim->texDivergence.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 6. Jacobi
  rlEnableShader(sim->shdJacobi);
  for (int i = 0; i < 40; i++) {
    glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, sim->texDivergence.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(2, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(3, sim->texPressure[1].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_R16F);
    rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    Texture2D_GL tmp = sim->texPressure[0];
    sim->texPressure[0] = sim->texPressure[1];
    sim->texPressure[1] = tmp;
  }

  // 7. Subtract
  rlEnableShader(sim->shdSubtract);
  glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA16F);
  glBindImageTexture(2, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();

  // --- AUTO-RANGE ANALYSIS ---
  unsigned int zeroStats[3] = {0, 0, 0};
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboStats);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zeroStats), zeroStats);

  rlEnableShader(sim->shdAnalyze);
  glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(2, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sim->ssboStats);
  rlSetUniform(rlGetLocationUniform(sim->shdAnalyze, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  rlDisableShader();

  unsigned int readStats[3];
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(readStats), readStats);

  float rawMaxP = 0.0f;
  float rawMaxV = 0.0f;
  float rawMaxC = 0.0f;
  memcpy(&rawMaxP, &readStats[0], sizeof(float));
  memcpy(&rawMaxV, &readStats[1], sizeof(float));
  memcpy(&rawMaxC, &readStats[2], sizeof(float));

  if (rawMaxP < 0.0001f) rawMaxP = 0.0001f;
  if (rawMaxV < 0.0001f) rawMaxV = 0.0001f;
  if (rawMaxC < 0.0001f) rawMaxC = 0.0001f;

  sim->maxPressureSmooth = sim->maxPressureSmooth * 0.95f + rawMaxP * 0.05f;
  sim->maxVelocitySmooth = sim->maxVelocitySmooth * 0.95f + rawMaxV * 0.05f;
  sim->maxCurlSmooth     = sim->maxCurlSmooth     * 0.95f + rawMaxC * 0.05f;

  // --- Force Calculation ---
  if (sim->enableWindTunnel) {
    int zero[2] = {0, 0};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), zero);
    rlEnableShader(sim->shdForce);
    glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssboForce);
    rlSetUniform(rlGetLocationUniform(sim->shdForce, "res"), &res,
                 RL_SHADER_UNIFORM_VEC2, 1);
    rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    rlDisableShader();
  }
}

Vector2 GetAerodynamicForces(FluidSim *sim) {
  if (!sim->enableWindTunnel)
    return (Vector2){0, 0};
  int data[2];
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data);
  float rawDrag = (float)data[0] / 100.0f;
  float rawLift = (float)data[1] / 100.0f;
  float visualScale = 0.005f;
  return (Vector2){rawDrag * visualScale, rawLift * visualScale};
}
