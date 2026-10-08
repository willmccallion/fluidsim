#include "palette.h"
#include <string.h>

static const Palette PALETTES[] = {
    {"spectrum",
     5,
     {{0.00f, {0.00f, 0.00f, 0.80f}},
      {0.25f, {0.00f, 0.80f, 1.00f}},
      {0.50f, {0.00f, 0.90f, 0.00f}},
      {0.75f, {1.00f, 0.90f, 0.00f}},
      {1.00f, {1.00f, 0.10f, 0.00f}}}},
    {"inferno",
     5,
     {{0.00f, {0.26f, 0.04f, 0.41f}},
      {0.35f, {0.58f, 0.15f, 0.40f}},
      {0.55f, {0.85f, 0.27f, 0.23f}},
      {0.75f, {0.98f, 0.55f, 0.04f}},
      {1.00f, {0.99f, 0.95f, 0.64f}}}},
    {"blood",
     4,
     {{0.00f, {0.30f, 0.00f, 0.03f}},
      {0.50f, {0.80f, 0.04f, 0.06f}},
      {0.80f, {1.00f, 0.35f, 0.25f}},
      {1.00f, {1.00f, 0.92f, 0.85f}}}},
    {"ember",
     5,
     {{0.00f, {0.40f, 0.03f, 0.00f}},
      {0.45f, {0.90f, 0.25f, 0.00f}},
      {0.70f, {1.00f, 0.60f, 0.08f}},
      {0.90f, {1.00f, 0.88f, 0.45f}},
      {1.00f, {1.00f, 1.00f, 0.90f}}}},
    {"psych",
     4,
     {{0.00f, {0.30f, 0.02f, 0.55f}},
      {0.40f, {0.80f, 0.10f, 0.55f}},
      {0.65f, {1.00f, 0.45f, 0.20f}},
      {1.00f, {1.00f, 0.92f, 0.30f}}}},
    {"chrome-blood",
     5,
     {{0.00f, {0.25f, 0.32f, 0.45f}},
      {0.45f, {0.85f, 0.87f, 0.90f}},
      {0.60f, {1.00f, 1.00f, 1.00f}},
      {0.80f, {0.95f, 0.20f, 0.15f}},
      {1.00f, {0.75f, 0.00f, 0.02f}}}},
};

int PaletteCount(void) { return (int)(sizeof(PALETTES) / sizeof(PALETTES[0])); }

const Palette *PaletteAt(int index) { return &PALETTES[index]; }

const Palette *FindPalette(const char *name) {
  for (int i = 0; i < PaletteCount(); i++) {
    if (strcmp(PALETTES[i].name, name) == 0)
      return &PALETTES[i];
  }
  return NULL;
}

static Rgb MixRgb(Rgb a, Rgb b, float t) {
  return (Rgb){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
               a.b + (b.b - a.b) * t};
}

Rgb SamplePalette(const Palette *palette, float t) {
  const ColorStop *stops = palette->stops;
  int last = palette->stopCount - 1;
  if (t <= stops[0].position)
    return stops[0].color;
  if (t >= stops[last].position)
    return stops[last].color;

  int i = 0;
  while (t > stops[i + 1].position)
    i++;
  float span = stops[i + 1].position - stops[i].position;
  return MixRgb(stops[i].color, stops[i + 1].color,
                (t - stops[i].position) / span);
}
