#ifndef SIMULATION_H
#define SIMULATION_H

#include "raylib.h"
#include "utils.h"

#define RES_X 128
#define RES_Y 64
#define RES_Z 64

typedef struct {
  // Ping-Pong Textures
  Texture3D texDensity[2];
  Texture3D texVelocity[2];
  Texture3D texPressure[2];
  Texture3D texDivergence;
  Texture3D texObstacles;

  // Compute Shaders
  unsigned int shdAdvect;
  unsigned int shdDivergence;
  unsigned int shdJacobi;
  unsigned int shdSubtract;

  int ping;
} FluidSim;

FluidSim InitFluidSim(void);
void UpdateFluidSim(FluidSim *sim, float dt, float time);
void CleanupFluidSim(FluidSim *sim);

#endif
