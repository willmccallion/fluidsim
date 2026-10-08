#include "streakline_image.h"
#include <math.h>
#include <stdlib.h>

typedef struct {
  float r, g, b;
} Rgb;

/** CPU copy of the simulation fields needed to draw one frame. */
typedef struct {
  const float *velocityRG;
  const float *smokeRGB;
  const float *obstacles;
  int width, height;
} FieldSnapshot;

static const Rgb BLACK_RGB = {0.0f, 0.0f, 0.0f};
static const Rgb OBSTACLE_RGB = {0.16f, 0.16f, 0.16f};

static Rgb MixRgb(Rgb a, Rgb b, float t) {
  return (Rgb){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
               a.b + (b.b - a.b) * t};
}

static float Clamp01(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }

/** Mirrors getSpectrum() in resources/shaders/display.fs. */
static Rgb Spectrum(float t) {
  const Rgb blue = {0.0f, 0.0f, 0.8f};
  const Rgb cyan = {0.0f, 0.8f, 1.0f};
  const Rgb green = {0.0f, 0.9f, 0.0f};
  const Rgb yellow = {1.0f, 0.9f, 0.0f};
  const Rgb red = {1.0f, 0.1f, 0.0f};

  t = Clamp01(t);
  if (t < 0.25f)
    return MixRgb(blue, cyan, t * 4.0f);
  if (t < 0.5f)
    return MixRgb(cyan, green, (t - 0.25f) * 4.0f);
  if (t < 0.75f)
    return MixRgb(green, yellow, (t - 0.5f) * 4.0f);
  return MixRgb(yellow, red, (t - 0.75f) * 4.0f);
}

static bool IsObstacle(float mask) { return mask > 0.5f; }

static float SpeedAt(const FieldSnapshot *f, int cell) {
  return hypotf(f->velocityRG[2 * cell], f->velocityRG[2 * cell + 1]);
}

/** Smoke concentration (0..1) at a cell; the inlet writes equal RGB. */
static float SmokeOpacityAt(const FieldSnapshot *f, int cell) {
  const float *rgb = &f->smokeRGB[3 * cell];
  return Clamp01(fmaxf(rgb[0], fmaxf(rgb[1], rgb[2])));
}

static float MaxFluidSpeed(const FieldSnapshot *f) {
  float maxSpeed = 0.0f;
  for (int cell = 0; cell < f->width * f->height; cell++) {
    if (!IsObstacle(f->obstacles[cell]))
      maxSpeed = fmaxf(maxSpeed, SpeedAt(f, cell));
  }
  return maxSpeed;
}

static Rgb StreaklineColorAt(const FieldSnapshot *f, int cell, float maxSpeed) {
  if (IsObstacle(f->obstacles[cell]))
    return OBSTACLE_RGB;
  return MixRgb(BLACK_RGB, Spectrum(SpeedAt(f, cell) / maxSpeed),
                SmokeOpacityAt(f, cell));
}

static unsigned char ToByte(float channel) {
  return (unsigned char)lroundf(Clamp01(channel) * 255.0f);
}

/** Image row 0 is the top of the domain. */
static Image RenderStreaklines(const FieldSnapshot *f) {
  float maxSpeed = fmaxf(MaxFluidSpeed(f), 0.0001f);
  unsigned char *pixels = RL_MALLOC((size_t)f->width * f->height * 4);
  if (pixels == NULL)
    return (Image){0};

  for (int y = 0; y < f->height; y++) {
    int imageRow = f->height - 1 - y;
    for (int x = 0; x < f->width; x++) {
      Rgb color = StreaklineColorAt(f, y * f->width + x, maxSpeed);
      unsigned char *px = &pixels[4 * (imageRow * f->width + x)];
      px[0] = ToByte(color.r);
      px[1] = ToByte(color.g);
      px[2] = ToByte(color.b);
      px[3] = 255;
    }
  }

  return (Image){.data = pixels,
                 .width = f->width,
                 .height = f->height,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}

static void ReadTexture(Texture2D_GL tex, GLenum format, float *out) {
  glBindTexture(GL_TEXTURE_2D, tex.id);
  glGetTexImage(GL_TEXTURE_2D, 0, format, GL_FLOAT, out);
}

bool ExportStreaklineImage(const FluidSim *sim, const char *path) {
  size_t cellCount = (size_t)RES_X * RES_Y;
  float *velocityRG = malloc(cellCount * 2 * sizeof(float));
  float *smokeRGB = malloc(cellCount * 3 * sizeof(float));
  float *obstacles = malloc(cellCount * sizeof(float));
  bool ok = false;

  if (velocityRG != NULL && smokeRGB != NULL && obstacles != NULL) {
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    ReadTexture(sim->texVelocity[sim->velocityPing], GL_RG, velocityRG);
    ReadTexture(sim->texDensity[sim->densityPing], GL_RGB, smokeRGB);
    ReadTexture(sim->texObstacles, GL_RED, obstacles);

    FieldSnapshot snapshot = {velocityRG, smokeRGB, obstacles, RES_X, RES_Y};
    Image image = RenderStreaklines(&snapshot);
    ok = image.data != NULL && ExportImage(image, path);
    UnloadImage(image);
  }

  free(velocityRG);
  free(smokeRGB);
  free(obstacles);
  return ok;
}
