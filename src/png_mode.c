#include "png_mode.h"
#include "headless.h"
#include "progress.h"
#include "simulation.h"
#include "streakline_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define STEP_SECONDS 0.005f
#define KMH_PER_SPEED_UNIT (350.0f / 4000.0f)
#define SQUARE_FRAME_CIRCLE_FRACTION 0.22f
#define SEARCH_INTERVAL_STEPS 20

/** Tracks the most turbulent frame seen while the simulation runs. */
typedef struct {
  CellRect crop;
  float *curl;
  float *obstacles;
  StreaklineFields best;
  int bestStep;
  double bestScore;
} FrameSearch;

/** Square frames put the circle near the left so the wake fills the rest. */
static CellRect FrameRect(FrameKind frame, int width, int height) {
  if (frame == FRAME_WIDE)
    return (CellRect){0, 0, width, height};

  int size = height;
  float circleX = (float)width * CIRCLE_SCENE_CENTER_X_FRACTION;
  int x = (int)lroundf(circleX - SQUARE_FRAME_CIRCLE_FRACTION * (float)size);
  if (x < 0)
    x = 0;
  if (x > width - size)
    x = width - size;
  return (CellRect){x, 0, size, size};
}

static void ApplyOptions(FluidSim *sim, const PngOptions *o) {
  sim->smokeLineCount = o->smokeLineCount;
  sim->smokeLineHalfWidth = o->smokeLineWidth * 0.5f * (float)o->scale;
  sim->windSpeed = o->windKmh / KMH_PER_SPEED_UNIT * (float)o->scale;
  sim->vorticityStrength = o->vorticityStrength;
  sim->pressureVCycles = o->pressureVCycles;
  sim->trackDisplayStats = false;
}

/** Mean |vorticity| over the fluid inside the crop. */
static double MeanAbsVorticity(const float *curl, const float *obstacles,
                               int fieldWidth, CellRect crop) {
  double sum = 0.0;
  long count = 0;
  for (int y = crop.y; y < crop.y + crop.height; y++) {
    for (int x = crop.x; x < crop.x + crop.width; x++) {
      long cell = (long)y * fieldWidth + x;
      if (obstacles[cell] > 0.5f)
        continue;
      sum += fabsf(curl[cell]);
      count++;
    }
  }
  return count > 0 ? sum / (double)count : 0.0;
}

static bool StartFrameSearch(const FluidSim *sim, CellRect crop,
                             FrameSearch *out) {
  size_t cellCount = (size_t)sim->width * sim->height;
  FrameSearch search = {.crop = crop,
                        .curl = malloc(cellCount * sizeof(float)),
                        .obstacles = malloc(cellCount * sizeof(float)),
                        .bestStep = 0,
                        .bestScore = -1.0};
  if (search.curl == NULL || search.obstacles == NULL) {
    free(search.curl);
    free(search.obstacles);
    return false;
  }
  ReadTextureFloats(sim->texObstacles, GL_RED, search.obstacles);
  *out = search;
  return true;
}

static void FreeFrameSearch(FrameSearch *search) {
  free(search->curl);
  free(search->obstacles);
  if (search->bestStep > 0)
    FreeStreaklineFields(&search->best);
}

/** Keeps a copy of this frame if it is more turbulent than the best so far. */
static bool ConsiderFrame(FrameSearch *search, const FluidSim *sim, int step) {
  ReadTextureFloats(sim->texCurl, GL_RED, search->curl);
  double score = MeanAbsVorticity(search->curl, search->obstacles, sim->width,
                                  search->crop);
  if (score <= search->bestScore)
    return true;

  StreaklineFields fields;
  if (!ReadStreaklineFields(sim, &fields))
    return false;
  if (search->bestStep > 0)
    FreeStreaklineFields(&search->best);
  search->best = fields;
  search->bestStep = step;
  search->bestScore = score;
  return true;
}

static bool IsSearchStep(int step, const PngOptions *o) {
  int stepsFromEnd = o->steps - step;
  return stepsFromEnd == 0 || (stepsFromEnd < o->searchSteps &&
                               stepsFromEnd % SEARCH_INTERVAL_STEPS == 0);
}

static bool Simulate(FluidSim *sim, const PngOptions *o, FrameSearch *search) {
  float time = 0.0f;
  ProgressBar progress = StartProgress(o->steps);
  for (int step = 1; step <= o->steps; step++) {
    time += STEP_SECONDS;
    UpdateSimSubstepped(sim, STEP_SECONDS, time,
                        o->velocitySubsteps * o->scale);
    // Unbounded queued GPU work made the final readback return zeros.
    glFinish();
    if (IsSearchStep(step, o) && !ConsiderFrame(search, sim, step))
      return false;
    UpdateProgress(&progress, step);
  }
  FinishProgress(&progress);
  return true;
}

static bool ExportPalette(const StreaklineFields *fields,
                          const Palette *palette, const PngOptions *o) {
  char path[1024];
  int length = snprintf(path, sizeof(path), "%s/sphere-%s.png", o->outputDir,
                        palette->name);
  if (length < 0 || (size_t)length >= sizeof(path)) {
    TraceLog(LOG_ERROR, "PNG: Output path too long for %s", palette->name);
    return false;
  }
  CellRect crop = FrameRect(o->frame, fields->width, fields->height);
  if (!ExportStreaklineImage(fields, palette, crop, o->supersample, path)) {
    TraceLog(LOG_ERROR, "PNG: Failed to write %s", path);
    return false;
  }
  printf("wrote %s\n", path);
  return true;
}

static bool ExportPalettes(const StreaklineFields *fields,
                           const PngOptions *o) {
  if (MakeDirectory(o->outputDir) != 0) {
    TraceLog(LOG_ERROR, "PNG: Cannot create %s", o->outputDir);
    return false;
  }
  if (o->palette != NULL)
    return ExportPalette(fields, o->palette, o);

  bool ok = true;
  for (int i = 0; i < PaletteCount(); i++)
    ok = ExportPalette(fields, PaletteAt(i), o) && ok;
  return ok;
}

int RunPngMode(const PngOptions *options) {
  SetTraceLogLevel(LOG_WARNING);
  HeadlessContext ctx;
  if (!InitHeadlessContext(&ctx))
    return 1;
  printf("GPU: %s\n", (const char *)glGetString(GL_RENDERER));

  FluidSim sim;
  InitSim(&sim, DEFAULT_SIM_WIDTH * options->scale,
          DEFAULT_SIM_HEIGHT * options->scale);
  ResetSim(&sim, SCENE_CIRCLE_WIND_TUNNEL);
  ApplyOptions(&sim, options);

  FrameSearch search;
  CellRect crop = FrameRect(options->frame, sim.width, sim.height);
  if (!StartFrameSearch(&sim, crop, &search)) {
    CloseHeadlessContext(&ctx);
    return 1;
  }

  bool ok = Simulate(&sim, options, &search);
  if (ok && options->searchSteps > 0)
    printf("chose step %d of %d (mean |vorticity| %.1f)\n", search.bestStep,
           options->steps, search.bestScore);
  if (ok)
    ok = ExportPalettes(&search.best, options);
  FreeFrameSearch(&search);
  CloseHeadlessContext(&ctx);
  return ok ? 0 : 1;
}
