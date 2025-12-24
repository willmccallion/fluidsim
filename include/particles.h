#ifndef PARTICLES_H
#define PARTICLES_H

#include "simulation.h"
#include "utils.h"

#define MAX_PARTICLES 50000

typedef struct {
  Vector2 pos;
  Vector2 vel;
  float life;
  float padding;
} Particle;

typedef struct {
  unsigned int ssbo;
  unsigned int shdUpdate;
} ParticleSys;

void InitParticles(ParticleSys *sys);
void UpdateParticles(ParticleSys *sys, FluidSim *sim, float dt, float time);
void DrawParticles(ParticleSys *sys);
void SeedWindTunnelParticles(ParticleSys *sys, float dt);

#endif
