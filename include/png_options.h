#ifndef PNG_OPTIONS_H
#define PNG_OPTIONS_H

#include "palette.h"
#include <stddef.h>
#include <stdio.h>

typedef enum {
  FRAME_SQUARE,
  FRAME_WIDE,
} FrameKind;

typedef struct {
  int steps;
  /** NULL renders every palette. */
  const Palette *palette;
  FrameKind frame;
  int smokeLineCount;
  float smokeLineWidth;
  float windKmh;
  float vorticityStrength;
  int velocitySubsteps;
  int pressureVCycles;
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
