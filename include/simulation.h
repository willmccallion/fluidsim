#ifndef SIMULATION_H
#define SIMULATION_H

#include "obstacle_shape.h"
#include "pressure.h"
#include "utils.h"

#define DEFAULT_SIM_WIDTH 2560
#define DEFAULT_SIM_HEIGHT 1280

typedef struct {
  int width;
  int height;
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
  float inletTurbulence;
  float inletSeed;
} FluidSim;

typedef enum {
  SCENE_FREE = 0,
  SCENE_CAR_WIND_TUNNEL = 1,
  SCENE_CIRCLE_WIND_TUNNEL = 2,
} SimScene;

void InitSim(FluidSim *sim, int width, int height);
void ResetSim(FluidSim *sim, SimScene scene);
/** Replaces all obstacles with `shape`; on failure they are left as is. */
bool PlaceObstacle(FluidSim *sim, const ObstacleShape *shape);
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
