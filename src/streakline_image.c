#include "streakline_image.h"
#include <math.h>
#include <stdlib.h>

static const Rgb BLACK_RGB = {0.0f, 0.0f, 0.0f};

static float Clamp01(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }

static bool IsObstacle(float mask) { return mask > 0.5f; }

static float SpeedAt(const StreaklineFields *f, int cell) {
  return hypotf(f->velocityRG[2 * cell], f->velocityRG[2 * cell + 1]);
}

/** Smoke concentration (0..1) at a cell; the inlet writes equal RGB. */
static float SmokeOpacityAt(const StreaklineFields *f, int cell) {
  const float *rgb = &f->smokeRGB[3 * cell];
  return Clamp01(fmaxf(rgb[0], fmaxf(rgb[1], rgb[2])));
}

static float MaxFluidSpeed(const StreaklineFields *f) {
  float maxSpeed = 0.0f;
  for (int cell = 0; cell < f->width * f->height; cell++) {
    if (!IsObstacle(f->obstacles[cell]))
      maxSpeed = fmaxf(maxSpeed, SpeedAt(f, cell));
  }
  return maxSpeed;
}

static Rgb StreaklineColorAt(const StreaklineFields *f, const Palette *palette,
                             int cell, float maxSpeed) {
  if (IsObstacle(f->obstacles[cell]))
    return BLACK_RGB;
  Rgb color = SamplePalette(palette, SpeedAt(f, cell) / maxSpeed);
  float opacity = SmokeOpacityAt(f, cell);
  return (Rgb){color.r * opacity, color.g * opacity, color.b * opacity};
}

static unsigned char ToByte(float channel) {
  return (unsigned char)lroundf(Clamp01(channel) * 255.0f);
}

/** Image row 0 is the top of the crop. */
static Image RenderStreaklines(const StreaklineFields *f,
                               const Palette *palette, CellRect crop) {
  float maxSpeed = fmaxf(MaxFluidSpeed(f), 0.0001f);
  unsigned char *pixels = RL_MALLOC((size_t)crop.width * crop.height * 4);
  if (pixels == NULL)
    return (Image){0};

  for (int row = 0; row < crop.height; row++) {
    int y = crop.y + crop.height - 1 - row;
    for (int col = 0; col < crop.width; col++) {
      int cell = y * f->width + crop.x + col;
      Rgb color = StreaklineColorAt(f, palette, cell, maxSpeed);
      unsigned char *px = &pixels[4 * (row * crop.width + col)];
      px[0] = ToByte(color.r);
      px[1] = ToByte(color.g);
      px[2] = ToByte(color.b);
      px[3] = 255;
    }
  }

  return (Image){.data = pixels,
                 .width = crop.width,
                 .height = crop.height,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}

static bool CropFitsFields(const StreaklineFields *f, CellRect crop) {
  return crop.x >= 0 && crop.y >= 0 && crop.width > 0 && crop.height > 0 &&
         crop.x + crop.width <= f->width && crop.y + crop.height <= f->height;
}

bool ReadStreaklineFields(const FluidSim *sim, StreaklineFields *out) {
  size_t cellCount = (size_t)sim->width * sim->height;
  StreaklineFields f = {.velocityRG = malloc(cellCount * 2 * sizeof(float)),
                        .smokeRGB = malloc(cellCount * 3 * sizeof(float)),
                        .obstacles = malloc(cellCount * sizeof(float)),
                        .width = sim->width,
                        .height = sim->height};
  if (f.velocityRG == NULL || f.smokeRGB == NULL || f.obstacles == NULL) {
    FreeStreaklineFields(&f);
    return false;
  }

  glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
  ReadTextureFloats(sim->texVelocity[sim->velocityPing], GL_RG, f.velocityRG);
  ReadTextureFloats(sim->texDensity[sim->densityPing], GL_RGB, f.smokeRGB);
  ReadTextureFloats(sim->texObstacles, GL_RED, f.obstacles);
  *out = f;
  return true;
}

void FreeStreaklineFields(StreaklineFields *fields) {
  free(fields->velocityRG);
  free(fields->smokeRGB);
  free(fields->obstacles);
  *fields = (StreaklineFields){0};
}

bool ExportStreaklineImage(const StreaklineFields *fields,
                           const Palette *palette, CellRect crop,
                           const char *path) {
  if (!CropFitsFields(fields, crop))
    return false;

  Image image = RenderStreaklines(fields, palette, crop);
  if (image.data == NULL)
    return false;

  bool ok = ExportImage(image, path);
  UnloadImage(image);
  return ok;
}
