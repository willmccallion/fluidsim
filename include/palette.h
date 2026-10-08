#ifndef PALETTE_H
#define PALETTE_H

#define MAX_COLOR_STOPS 8

typedef struct {
  float r, g, b;
} Rgb;

typedef struct {
  float position;
  Rgb color;
} ColorStop;

/** Piecewise-linear colour ramp over [0, 1]; stops sorted by position. */
typedef struct {
  const char *name;
  int stopCount;
  ColorStop stops[MAX_COLOR_STOPS];
} Palette;

int PaletteCount(void);
const Palette *PaletteAt(int index);

/** Returns the palette with this name, or NULL if there is none. */
const Palette *FindPalette(const char *name);

Rgb SamplePalette(const Palette *palette, float t);

#endif
