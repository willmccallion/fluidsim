#include "png_mode.h"
#include "headless.h"
#include "progress.h"
#include "simulation.h"
#include "streakline_image.h"
#include <math.h>
#include <stdio.h>

#define STEP_SECONDS 0.005f
#define KMH_PER_SPEED_UNIT (350.0f / 4000.0f)
#define SQUARE_FRAME_CIRCLE_FRACTION 0.22f

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

static void Simulate(FluidSim *sim, const PngOptions *o) {
  float time = 0.0f;
  ProgressBar progress = StartProgress(o->steps);
  for (int step = 1; step <= o->steps; step++) {
    time += STEP_SECONDS;
    UpdateSimSubstepped(sim, STEP_SECONDS, time,
                        o->velocitySubsteps * o->scale);
    // Unbounded queued GPU work made the final readback return zeros.
    glFinish();
    UpdateProgress(&progress, step);
  }
  FinishProgress(&progress);
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
  if (!ExportStreaklineImage(fields, palette, crop, path)) {
    TraceLog(LOG_ERROR, "PNG: Failed to write %s", path);
    return false;
  }
  printf("wrote %s\n", path);
  return true;
}

static bool ExportPalettes(const FluidSim *sim, const PngOptions *o) {
  if (MakeDirectory(o->outputDir) != 0) {
    TraceLog(LOG_ERROR, "PNG: Cannot create %s", o->outputDir);
    return false;
  }

  StreaklineFields fields;
  if (!ReadStreaklineFields(sim, &fields))
    return false;

  bool ok = true;
  if (o->palette != NULL) {
    ok = ExportPalette(&fields, o->palette, o);
  } else {
    for (int i = 0; i < PaletteCount(); i++)
      ok = ExportPalette(&fields, PaletteAt(i), o) && ok;
  }
  FreeStreaklineFields(&fields);
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
  Simulate(&sim, options);

  bool exported = ExportPalettes(&sim, options);
  CloseHeadlessContext(&ctx);
  return exported ? 0 : 1;
}
