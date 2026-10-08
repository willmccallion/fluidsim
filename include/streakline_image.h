#ifndef STREAKLINE_IMAGE_H
#define STREAKLINE_IMAGE_H

#include "palette.h"
#include "simulation.h"

/** CPU copy of the simulation fields the streakline renderer needs. */
typedef struct {
  float *velocityRG;
  float *smokeRGB;
  float *obstacles;
  int width, height;
} StreaklineFields;

/** Cell rectangle in simulation space (y = 0 is the bottom row). */
typedef struct {
  int x, y, width, height;
} CellRect;

bool ReadStreaklineFields(const FluidSim *sim, StreaklineFields *out);
void FreeStreaklineFields(StreaklineFields *fields);

/**
 * Writes the smoke lines inside `crop` as a PNG on black, coloured by local
 * speed through `palette` (slow = 0, fastest = 1). Each pixel averages
 * samplesPerAxis^2 bilinear samples to smooth edges.
 */
bool ExportStreaklineImage(const StreaklineFields *fields,
                           const Palette *palette, CellRect crop,
                           int samplesPerAxis, const char *path);

#endif
