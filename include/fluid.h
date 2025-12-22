#ifndef FLUID_H
#define FLUID_H

#include <stdbool.h>

#define N 128
#define ITER 10 // Increased iterations for better stability
#define IX(x, y) ((x) + (y) * N)

typedef struct {
  int size;
  float dt;
  float diff;
  float visc;

  float *s;
  float *density;

  float *Vx;
  float *Vy;
  float *Vx0;
  float *Vy0;

  bool *obstacles;
} Fluid;

Fluid *Fluid_Create(float dt, float diffusion, float viscosity);
void Fluid_Free(Fluid *fluid);

void Fluid_AddCarObstacle(Fluid *fluid);
void Fluid_Step(Fluid *fluid);
void Fluid_InjectStreamlines(Fluid *fluid, int interval);

#endif
