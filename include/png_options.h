#ifndef PNG_OPTIONS_H
#define PNG_OPTIONS_H

#include "obstacle_shape.h"
#include "palette.h"
#include <stddef.h>
#include <stdio.h>

typedef enum {
  FRAME_SQUARE,
  FRAME_WIDE,
} FrameKind;

typedef struct {
  int steps;
  /** Grid multiplier over the 2560x1280 default; image detail scales too. */
  int scale;
  /** NULL renders every palette. */
  const Palette *palette;
  FrameKind frame;
  ObstacleShape shape;
  int smokeLineCount;
  /** Line width in default-grid cells; multiplied by `scale`. */
  float smokeLineWidth;
  float windKmh;
  float vorticityStrength;
  /** Substeps at scale 1; multiplied by `scale` to keep the CFL number. */
  int velocitySubsteps;
  int pressureVCycles;
  /** Export the most turbulent frame of the last N steps; 0 = final step. */
  int searchSteps;
  /** Samples per pixel axis when colouring; 1 = one sample per cell. */
  int supersample;
  const char *outputDir;
} PngOptions;

typedef enum {
  PNG_ARGS_OK,
  PNG_ARGS_HELP,
  PNG_ARGS_INVALID,
} PngArgsResult;

PngOptions DefaultPngOptions(void);

/**
 * Parses the arguments that follow `--png`. On PNG_ARGS_INVALID a message is
 * written to `error`.
 */
PngArgsResult ParsePngOptions(int argc, char *const argv[], PngOptions *out,
                              char *error, size_t errorSize);

void PrintPngUsage(FILE *stream, const char *program);

#endif
