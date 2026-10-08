#include "png_options.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

PngOptions DefaultPngOptions(void) {
  return (PngOptions){.steps = 2000,
                      .palette = NULL,
                      .frame = FRAME_SQUARE,
                      .smokeLineCount = 60,
                      .smokeLineWidth = 5.0f,
                      .windKmh = 250.0f,
                      .vorticityStrength = 5.0f,
                      .velocitySubsteps = 4,
                      .pressureVCycles = 2,
                      .outputDir = "output"};
}

static bool ParseIntInRange(const char *text, int min, int max, int *out) {
  char *end = NULL;
  errno = 0;
  long value = strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || value < min || value > max)
    return false;
  *out = (int)value;
  return true;
}

static bool ParseFloatInRange(const char *text, float min, float max,
                              float *out) {
  char *end = NULL;
  errno = 0;
  float value = strtof(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !(value >= min) ||
      !(value <= max))
    return false;
  *out = value;
  return true;
}

static bool ParseFrame(const char *text, FrameKind *out) {
  if (strcmp(text, "square") == 0) {
    *out = FRAME_SQUARE;
    return true;
  }
  if (strcmp(text, "wide") == 0) {
    *out = FRAME_WIDE;
    return true;
  }
  return false;
}

static bool ParsePalette(const char *text, const Palette **out) {
  if (strcmp(text, "all") == 0) {
    *out = NULL;
    return true;
  }
  const Palette *palette = FindPalette(text);
  if (palette == NULL)
    return false;
  *out = palette;
  return true;
}

static bool ParseSteps(const char *v, PngOptions *o) {
  return ParseIntInRange(v, 1, 1000000, &o->steps);
}
static bool ParsePaletteOption(const char *v, PngOptions *o) {
  return ParsePalette(v, &o->palette);
}
static bool ParseFrameOption(const char *v, PngOptions *o) {
  return ParseFrame(v, &o->frame);
}
static bool ParseLines(const char *v, PngOptions *o) {
  return ParseIntInRange(v, 1, 640, &o->smokeLineCount);
}
static bool ParseLineWidth(const char *v, PngOptions *o) {
  return ParseFloatInRange(v, 0.5f, 100.0f, &o->smokeLineWidth);
}
static bool ParseWind(const char *v, PngOptions *o) {
  return ParseFloatInRange(v, 10.0f, 1000.0f, &o->windKmh);
}
static bool ParseVorticity(const char *v, PngOptions *o) {
  return ParseFloatInRange(v, 0.0f, 100.0f, &o->vorticityStrength);
}
static bool ParseSubsteps(const char *v, PngOptions *o) {
  return ParseIntInRange(v, 1, 64, &o->velocitySubsteps);
}
static bool ParseVCycles(const char *v, PngOptions *o) {
  return ParseIntInRange(v, 1, 32, &o->pressureVCycles);
}
static bool ParseOutputDir(const char *v, PngOptions *o) {
  o->outputDir = v;
  return v[0] != '\0';
}

typedef struct {
  const char *flag;
  bool (*parse)(const char *value, PngOptions *options);
} OptionSpec;

static const OptionSpec OPTION_SPECS[] = {
    {"--steps", ParseSteps},         {"--palette", ParsePaletteOption},
    {"--frame", ParseFrameOption},   {"--lines", ParseLines},
    {"--line-width", ParseLineWidth}, {"--wind-kmh", ParseWind},
    {"--vorticity", ParseVorticity}, {"--substeps", ParseSubsteps},
    {"--vcycles", ParseVCycles},     {"--out", ParseOutputDir},
};

static const OptionSpec *FindOptionSpec(const char *flag) {
  for (size_t i = 0; i < sizeof(OPTION_SPECS) / sizeof(OPTION_SPECS[0]); i++) {
    if (strcmp(flag, OPTION_SPECS[i].flag) == 0)
      return &OPTION_SPECS[i];
  }
  return NULL;
}

PngArgsResult ParsePngOptions(int argc, char *const argv[], PngOptions *out,
                              char *error, size_t errorSize) {
  PngOptions options = DefaultPngOptions();
  for (int i = 0; i < argc; i++) {
    const char *flag = argv[i];
    if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0)
      return PNG_ARGS_HELP;
    const OptionSpec *spec = FindOptionSpec(flag);
    if (spec == NULL) {
      snprintf(error, errorSize, "unknown option '%s'", flag);
      return PNG_ARGS_INVALID;
    }
    if (i + 1 >= argc) {
      snprintf(error, errorSize, "missing value for %s", flag);
      return PNG_ARGS_INVALID;
    }
    const char *value = argv[++i];
    if (!spec->parse(value, &options)) {
      snprintf(error, errorSize, "invalid value '%s' for %s", value, flag);
      return PNG_ARGS_INVALID;
    }
  }
  *out = options;
  return PNG_ARGS_OK;
}

void PrintPngUsage(FILE *stream, const char *program) {
  PngOptions d = DefaultPngOptions();
  fprintf(stream,
          "usage: %s --png [options]\n"
          "\n"
          "Simulates flow past a circle without a window and writes\n"
          "<out>/sphere-<palette>.png for each requested palette.\n"
          "\n"
          "  --steps N         simulation steps of 0.005 s (default %d)\n"
          "  --palette NAME    palette name or 'all' (default all)\n"
          "  --frame KIND      square or wide (default square)\n"
          "  --lines N         inlet smoke lines (default %d)\n"
          "  --line-width PX   smoke line core width (default %.1f)\n"
          "  --wind-kmh N      inlet wind speed (default %.0f)\n"
          "  --vorticity X     vorticity confinement strength (default %.1f)\n"
          "  --substeps N      velocity substeps per step (default %d)\n"
          "  --vcycles N       multigrid V-cycles per substep (default %d)\n"
          "  --out DIR         output directory (default %s)\n"
          "\n"
          "palettes:",
          program, d.steps, d.smokeLineCount, d.smokeLineWidth, d.windKmh,
          d.vorticityStrength, d.velocitySubsteps, d.pressureVCycles,
          d.outputDir);
  for (int i = 0; i < PaletteCount(); i++)
    fprintf(stream, " %s", PaletteAt(i)->name);
  fprintf(stream, "\n");
}
