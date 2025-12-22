#include "simulation.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>

// Linux GL Headers
#if defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

void GenerateObstacles(FluidSim *sim) {
  int size = RES_X * RES_Y * RES_Z;
  float *data = (float *)calloc(size, sizeof(float));

  float cx = RES_X / 2.0f;
  float cz = RES_Z / 2.0f;
  float floorY = 6.0f;

  for (int z = 0; z < RES_Z; z++) {
    for (int y = 0; y < RES_Y; y++) {
      for (int x = 0; x < RES_X; x++) {

        bool solid = false;

        // F1 Car Approximation
        // 1. Main Body
        if (x > cx - 15 && x < cx + 15 && y >= floorY && y < floorY + 4 &&
            z > cz - 5 && z < cz + 5)
          solid = true;

        // 2. Nose
        if (x > cx - 35 && x < cx - 15 && y >= floorY) {
          float taper = (x - (cx - 35)) / 20.0f;
          float wTaper = 4.0f * taper;
          float hTaper = 1.5f + 1.5f * taper;
          if (y < floorY + hTaper && abs(z - cz) < wTaper)
            solid = true;
        }

        // 3. Rear Wing
        if (x > cx + 20 && x < cx + 25 && y > floorY + 6 && y < floorY + 10 &&
            z > cz - 10 && z < cz + 10)
          solid = true;
        // 4. Rear Wheels
        if (x > cx + 15 && x < cx + 25 && y >= floorY && y < floorY + 6 &&
            abs(z - cz) > 8 && abs(z - cz) < 12)
          solid = true;
        // 5. Front Wheels
        if (x > cx - 20 && x < cx - 10 && y >= floorY && y < floorY + 6 &&
            abs(z - cz) > 8 && abs(z - cz) < 12)
          solid = true;
        // 6. Driver Head
        if (x > cx - 5 && x < cx && y >= floorY + 4 && y < floorY + 7 &&
            z > cz - 2 && z < cz + 2)
          solid = true;

        if (solid)
          data[x + y * RES_X + z * RES_X * RES_Y] = 1.0f;
      }
    }
  }
  Upload3DData(sim->texObstacles, data, GL_RED);
  free(data);
}

FluidSim InitFluidSim(void) {
  FluidSim sim = {0};

  sim.texDensity[0] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);
  sim.texDensity[1] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);
  sim.texVelocity[0] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_RGBA16F);
  sim.texVelocity[1] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_RGBA16F);
  sim.texPressure[0] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);
  sim.texPressure[1] = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);
  sim.texDivergence = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);
  sim.texObstacles = CreateTexture3D(RES_X, RES_Y, RES_Z, GL_R16F);

  glBindTexture(GL_TEXTURE_3D, sim.texDensity[0].id);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // CHANGED: Use Linear filtering for obstacles to get smooth normals in shader
  glBindTexture(GL_TEXTURE_3D, sim.texObstacles.id);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  int numPixels = RES_X * RES_Y * RES_Z;
  float *zeroBuffer = (float *)calloc(numPixels * 4, sizeof(float));
  Upload3DData(sim.texDensity[0], zeroBuffer, GL_RED);
  Upload3DData(sim.texDensity[1], zeroBuffer, GL_RED);
  Upload3DData(sim.texPressure[0], zeroBuffer, GL_RED);
  Upload3DData(sim.texPressure[1], zeroBuffer, GL_RED);
  Upload3DData(sim.texDivergence, zeroBuffer, GL_RED);

  float *velBuffer = (float *)calloc(numPixels * 4, sizeof(float));
  for (int i = 0; i < numPixels; i++) {
    velBuffer[i * 4 + 0] = 40.0f;
    velBuffer[i * 4 + 1] = 0.0f;
    velBuffer[i * 4 + 2] = 0.0f;
    velBuffer[i * 4 + 3] = 0.0f;
  }
  Upload3DData(sim.texVelocity[0], velBuffer, GL_RGBA);
  Upload3DData(sim.texVelocity[1], velBuffer, GL_RGBA);

  free(zeroBuffer);
  free(velBuffer);

  sim.shdAdvect = LoadComputeShader("resources/shaders/fluid3d_advect.glsl");
  sim.shdDivergence = LoadComputeShader("resources/shaders/fluid3d_div.glsl");
  sim.shdJacobi = LoadComputeShader("resources/shaders/fluid3d_jacobi.glsl");
  sim.shdSubtract =
      LoadComputeShader("resources/shaders/fluid3d_subtract.glsl");

  GenerateObstacles(&sim);
  sim.ping = 0;
  return sim;
}

void CleanupFluidSim(FluidSim *sim) {}

void UpdateFluidSim(FluidSim *sim, float dt, float time) {
  // (Same as before, no changes needed here)
  if (sim->shdAdvect == 0)
    return;
  int steps = 3;
  float subDt = 0.005f;
  for (int k = 0; k < steps; k++) {
    int p = sim->ping;
    int next_p = !p;
    rlEnableShader(sim->shdAdvect);
    glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_RGBA16F);
    glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_RGBA16F);
    glBindImageTexture(2, sim->texVelocity[next_p].id, 0, GL_TRUE, 0,
                       GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(3, sim->texObstacles.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "dt"), &subDt,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "isVelocity"), &(int){1},
                 RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "time"), &time,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlComputeShaderDispatch(RES_X / 8, RES_Y / 8, RES_Z / 8);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glBindImageTexture(0, sim->texVelocity[next_p].id, 0, GL_TRUE, 0,
                       GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(4, sim->texDensity[p].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(5, sim->texDensity[next_p].id, 0, GL_TRUE, 0,
                       GL_WRITE_ONLY, GL_R16F);
    glBindImageTexture(3, sim->texObstacles.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "isVelocity"), &(int){0},
                 RL_SHADER_UNIFORM_INT, 1);
    rlComputeShaderDispatch(RES_X / 8, RES_Y / 8, RES_Z / 8);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    sim->ping = next_p;
    p = sim->ping;
    rlEnableShader(sim->shdDivergence);
    glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_RGBA16F);
    glBindImageTexture(1, sim->texObstacles.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(2, sim->texDivergence.id, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                       GL_R16F);
    rlComputeShaderDispatch(RES_X / 8, RES_Y / 8, RES_Z / 8);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    rlEnableShader(sim->shdJacobi);
    for (int i = 0; i < 40; i++) {
      glBindImageTexture(0, sim->texPressure[0].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                         GL_R16F);
      glBindImageTexture(1, sim->texDivergence.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                         GL_R16F);
      glBindImageTexture(2, sim->texObstacles.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                         GL_R16F);
      glBindImageTexture(3, sim->texPressure[1].id, 0, GL_TRUE, 0,
                         GL_WRITE_ONLY, GL_R16F);
      rlComputeShaderDispatch(RES_X / 8, RES_Y / 8, RES_Z / 8);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      Texture3D temp = sim->texPressure[0];
      sim->texPressure[0] = sim->texPressure[1];
      sim->texPressure[1] = temp;
    }
    rlEnableShader(sim->shdSubtract);
    glBindImageTexture(0, sim->texPressure[0].id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_TRUE, 0, GL_READ_WRITE,
                       GL_RGBA16F);
    glBindImageTexture(2, sim->texObstacles.id, 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_R16F);
    rlComputeShaderDispatch(RES_X / 8, RES_Y / 8, RES_Z / 8);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    rlDisableShader();
  }
}
