#ifndef SIMULATION_H
#define SIMULATION_H

#include "pressure.h"
#include "utils.h"

#define RES_X 2560
#define RES_Y 1280

#define CIRCLE_SCENE_CENTER_X (RES_X * 0.5f)
#define CIRCLE_SCENE_CENTER_Y (RES_Y * 0.5f)
#define CIRCLE_SCENE_RADIUS (RES_Y * 0.1f)

typedef struct {
  Texture2D_GL texDensity[2];
  Texture2D_GL texVelocity[2];
  Texture2D_GL texPressure;
  Texture2D_GL texDivergence;
  Texture2D_GL texCurl;
  Texture2D_GL texObstacles;

  unsigned int shdAdvect, shdDivergence, shdSubtract, shdCurl, shdVorticity;
  unsigned int shdSplat, shdInlet, shdPaint;
  unsigned int shdForce;

  // --- NEW: Analysis ---
  unsigned int shdAnalyze;
  unsigned int ssboStats;
  unsigned int ssboForce;
  PressureSolver pressure;

  // Smoothed stats for auto-exposure (prevents flickering)
  float maxPressureSmooth;
  float maxVelocitySmooth;
  float maxCurlSmooth;

  int velocityPing;
  int densityPing;
  bool enableWindTunnel;
  float buoyancyStrength;
  float windSpeed;
  int pressureVCycles;
  bool trackDisplayStats;
  float vorticityStrength;
  int smokeLineCount;
  float smokeLineHalfWidth;
} FluidSim;

typedef enum {
  SCENE_FREE = 0,
  SCENE_CAR_WIND_TUNNEL = 1,
  SCENE_CIRCLE_WIND_TUNNEL = 2,
} SimScene;

void InitSim(FluidSim *sim);
void ResetSim(FluidSim *sim, SimScene scene);
void UpdateSim(FluidSim *sim, float dt, float time);
/**
 * Advances by dt: smoke is advected once, velocity in `velocitySubsteps`
 * smaller steps. Fewer smoke resamples keep thin smoke lines from blurring.
 */
void UpdateSimSubstepped(FluidSim *sim, float dt, float time,
                         int velocitySubsteps);
void ApplySplat(FluidSim *sim, Texture2D_GL tex, Vector2 pos, float radius,
                Vector4 color);
void PaintObstacle(FluidSim *sim, Vector2 pos, float radius, bool erase);
Vector2 GetAerodynamicForces(FluidSim *sim);

#endif
