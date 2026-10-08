#include "streakline_image.h"
#include <math.h>
#include <stdlib.h>

static float Clamp01(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }

static bool IsObstacle(float mask) { return mask > 0.5f; }

static float SpeedAt(const StreaklineFields *f, int cell) {
  return hypotf(f->velocityRG[2 * cell], f->velocityRG[2 * cell + 1]);
}

static float MaxFluidSpeed(const StreaklineFields *f) {
  float maxSpeed = 0.0f;
  for (int cell = 0; cell < f->width * f->height; cell++) {
    if (!IsObstacle(f->obstacles[cell]))
      maxSpeed = fmaxf(maxSpeed, SpeedAt(f, cell));
  }
  return maxSpeed;
}

/** Field values at a point between cell centres. */
typedef struct {
  float speed;
  float opacity;
  float solidCoverage;
} FieldSample;

typedef struct {
  int x0, y0, x1, y1;
  float tx, ty;
} BilinearTaps;

static int ClampIndex(int i, int size) {
  if (i < 0)
    return 0;
  return i >= size ? size - 1 : i;
}

/** Cell i spans [i, i + 1), so its value sits at i + 0.5. */
static BilinearTaps TapsAt(const StreaklineFields *f, float x, float y) {
  float fx = x - 0.5f;
  float fy = y - 0.5f;
  float bx = floorf(fx);
  float by = floorf(fy);
  return (BilinearTaps){ClampIndex((int)bx, f->width),
                        ClampIndex((int)by, f->height),
                        ClampIndex((int)bx + 1, f->width),
                        ClampIndex((int)by + 1, f->height), fx - bx, fy - by};
}

static float Interpolate(const float *data, int stride, int channel,
                         int width, BilinearTaps t) {
  float v00 = data[((size_t)t.y0 * width + t.x0) * stride + channel];
  float v10 = data[((size_t)t.y0 * width + t.x1) * stride + channel];
  float v01 = data[((size_t)t.y1 * width + t.x0) * stride + channel];
  float v11 = data[((size_t)t.y1 * width + t.x1) * stride + channel];
  float bottom = v00 + (v10 - v00) * t.tx;
  float top = v01 + (v11 - v01) * t.tx;
  return bottom + (top - bottom) * t.ty;
}

static FieldSample SampleFields(const StreaklineFields *f, float x, float y) {
  BilinearTaps t = TapsAt(f, x, y);
  float vx = Interpolate(f->velocityRG, 2, 0, f->width, t);
  float vy = Interpolate(f->velocityRG, 2, 1, f->width, t);
  float smoke = fmaxf(Interpolate(f->smokeRGB, 3, 0, f->width, t),
                      fmaxf(Interpolate(f->smokeRGB, 3, 1, f->width, t),
                            Interpolate(f->smokeRGB, 3, 2, f->width, t)));
  return (FieldSample){hypotf(vx, vy), Clamp01(smoke),
                       Clamp01(Interpolate(f->obstacles, 1, 0, f->width, t))};
}

/** Smoke coloured by speed over black; obstacles stay black. */
static Rgb SampleColor(const StreaklineFields *f, const Palette *palette,
                       float maxSpeed, float x, float y) {
  FieldSample sample = SampleFields(f, x, y);
  Rgb color = SamplePalette(palette, sample.speed / maxSpeed);
  float weight = sample.opacity * (1.0f - sample.solidCoverage);
  return (Rgb){color.r * weight, color.g * weight, color.b * weight};
}

/** Averages samplesPerAxis^2 evenly spaced samples inside one cell. */
static Rgb PixelColor(const StreaklineFields *f, const Palette *palette,
                      float maxSpeed, int cellX, int cellY,
                      int samplesPerAxis) {
  Rgb sum = {0.0f, 0.0f, 0.0f};
  for (int sy = 0; sy < samplesPerAxis; sy++) {
    float y = (float)cellY + ((float)sy + 0.5f) / (float)samplesPerAxis;
    for (int sx = 0; sx < samplesPerAxis; sx++) {
      float x = (float)cellX + ((float)sx + 0.5f) / (float)samplesPerAxis;
      Rgb c = SampleColor(f, palette, maxSpeed, x, y);
      sum.r += c.r;
      sum.g += c.g;
      sum.b += c.b;
    }
  }
  float n = (float)(samplesPerAxis * samplesPerAxis);
  return (Rgb){sum.r / n, sum.g / n, sum.b / n};
}

static unsigned char ToByte(float channel) {
  return (unsigned char)lroundf(Clamp01(channel) * 255.0f);
}

/** Image row 0 is the top of the crop. */
static Image RenderStreaklines(const StreaklineFields *f,
                               const Palette *palette, CellRect crop,
                               int samplesPerAxis) {
  float maxSpeed = fmaxf(MaxFluidSpeed(f), 0.0001f);
  unsigned char *pixels = RL_MALLOC((size_t)crop.width * crop.height * 4);
  if (pixels == NULL)
    return (Image){0};

  for (int row = 0; row < crop.height; row++) {
    int cellY = crop.y + crop.height - 1 - row;
    for (int col = 0; col < crop.width; col++) {
      Rgb color = PixelColor(f, palette, maxSpeed, crop.x + col, cellY,
                             samplesPerAxis);
      unsigned char *px = &pixels[4 * ((size_t)row * crop.width + col)];
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
                           int samplesPerAxis, const char *path) {
  if (!CropFitsFields(fields, crop))
    return false;

  Image image = RenderStreaklines(fields, palette, crop, samplesPerAxis);
  if (image.data == NULL)
    return false;

  bool ok = ExportImage(image, path);
  UnloadImage(image);
  return ok;
}
