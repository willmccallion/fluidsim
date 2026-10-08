#ifndef PRESSURE_H
#define PRESSURE_H

#include "utils.h"

#define MAX_PRESSURE_LEVELS 12

typedef struct {
  Texture2D_GL pressure;
  Texture2D_GL rhs;
  Texture2D_GL mask;
} PressureLevel;

/**
 * Geometric multigrid solver for the pressure Poisson equation. Level 0 is
 * the simulation's own pressure, divergence and obstacle textures; each
 * coarser level halves both dimensions.
 */
typedef struct {
  PressureLevel levels[MAX_PRESSURE_LEVELS];
  int levelCount;
  unsigned int shdSmooth;
  unsigned int shdRestrict;
  unsigned int shdProlong;
  unsigned int shdRestrictMask;
} PressureSolver;

PressureSolver InitPressureSolver(Texture2D_GL pressure, Texture2D_GL divergence,
                                  Texture2D_GL obstacles);

/** Recomputes the coarse obstacle masks; call whenever obstacles change. */
void RebuildPressureMasks(const PressureSolver *solver);

/** Runs V-cycles on level 0, warm-started from the current pressure. */
void SolvePressure(const PressureSolver *solver, int vCycles);

#endif
