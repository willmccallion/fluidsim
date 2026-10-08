#ifndef STREAKLINE_IMAGE_H
#define STREAKLINE_IMAGE_H

#include "simulation.h"

/**
 * Writes the inlet smoke lines as a PNG on black, each pixel coloured by local
 * speed with the in-app velocity spectrum (blue slow, red fast).
 */
bool ExportStreaklineImage(const FluidSim *sim, const char *path);

#endif
