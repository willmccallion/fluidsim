#include "particles.h"
#include <stdlib.h>

// Ensure RES_Y is available
#ifndef RES_Y
#define RES_Y 1280
#endif

int particleHead = 0;

void InitParticles(ParticleSys *sys) {
  sys->shdUpdate =
      LoadCompute(LoadFileText("resources/shaders/particle_advect.glsl"));

  glGenBuffers(1, &sys->ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sys->ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL,
               GL_DYNAMIC_DRAW);

  // Initialize particles to dead state
  Particle *ptr =
      (Particle *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
  for (int i = 0; i < MAX_PARTICLES; i++) {
    ptr[i].pos = (Vector2){0, 0};
    ptr[i].life = -1.0f;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

void SeedWindTunnelParticles(ParticleSys *sys, float dt) {
  // Spawn 100 particles per frame for a dense stream
  int count = 100;
  Particle newParts[100];

  for (int i = 0; i < count; i++) {
    float y = (float)(rand() % RES_Y);
    // Spawn at inlet
    newParts[i].pos = (Vector2){5.0f, y};
    newParts[i].vel = (Vector2){1.0f, 0.0f};
    newParts[i].life = 1.0f;
    newParts[i].padding = 0.0f;
  }

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sys->ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, particleHead * sizeof(Particle),
                  count * sizeof(Particle), newParts);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  particleHead = (particleHead + count) % MAX_PARTICLES;
}

void UpdateParticles(ParticleSys *sys, FluidSim *sim, float dt, float time) {
  rlEnableShader(sys->shdUpdate);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sys->ssbo);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[sim->ping].id);

  Vector2 res = {(float)RES_X, (float)RES_Y};
  rlSetUniform(rlGetLocationUniform(sys->shdUpdate, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sys->shdUpdate, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sys->shdUpdate, "time"), &time,
               RL_SHADER_UNIFORM_FLOAT, 1);

  int numGroups = (MAX_PARTICLES + 255) / 256;
  rlComputeShaderDispatch(numGroups, 1, 1);
  rlDisableShader();
}

void DrawParticles(ParticleSys *sys) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sys->ssbo);
  Particle *parts =
      (Particle *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

  rlBegin(RL_QUADS);
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (parts[i].life > 0.0f) {
      float alpha = parts[i].life;
      if (alpha > 1.0f)
        alpha = 1.0f;

      rlColor4f(0.8f, 0.9f, 1.0f, alpha * 0.6f);

      float x = parts[i].pos.x;
      float y = RES_Y - parts[i].pos.y;
      float size = 1.5f;

      rlVertex2f(x - size, y - size);
      rlVertex2f(x + size, y - size);
      rlVertex2f(x + size, y + size);
      rlVertex2f(x - size, y + size);
    }
  }
  rlEnd();
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}
