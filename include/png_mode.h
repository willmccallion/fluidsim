#ifndef PNG_MODE_H
#define PNG_MODE_H

#include "png_options.h"

/**
 * Simulates flow past a circle without a window and writes one image per
 * requested palette. Returns a process exit code.
 */
int RunPngMode(const PngOptions *options);

#endif
