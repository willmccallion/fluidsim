#ifndef SIMULATION_H
#define SIMULATION_H

#include "utils.h"

#define RES_X 2560
#define RES_Y 1280

typedef struct {
  Texture2D_GL texDensity[2];
  Texture2D_GL texVelocity[2];
  Texture2D_GL texPressure[2];
  Texture2D_GL texDivergence;
  Texture2D_GL texCurl;
  Texture2D_GL texObstacles;

  unsigned int shdAdvect, shdDivergence, shdJacobi, shdSubtract, shdCurl,
      shdVorticity;
  unsigned int shdSplat, shdInlet, shdPaint;
  unsigned int shdForce;

  // --- NEW: Analysis ---
  unsigned int shdAnalyze;
  unsigned int ssboStats;
  unsigned int ssboForce;

  // Smoothed stats for auto-exposure (prevents flickering)
  float maxPressureSmooth;
  float maxVelocitySmooth;

  int ping;
  bool enableWindTunnel;
  float buoyancyStrength;
} FluidSim;

void InitSim(FluidSim *sim);
void ResetSim(FluidSim *sim, int mode);
void UpdateSim(FluidSim *sim, float dt, float time);
void ApplySplat(FluidSim *sim, Texture2D_GL tex, Vector2 pos, float radius,
                Vector4 color);
void PaintObstacle(FluidSim *sim, Vector2 pos, float radius, bool erase);
Vector2 GetAerodynamicForces(FluidSim *sim);

#endif
