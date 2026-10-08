#include "simulation.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int GroupCount(int cells) { return (unsigned int)(cells + 15) / 16; }

static void DispatchOverGrid(const FluidSim *sim) {
  rlComputeShaderDispatch(GroupCount(sim->width), GroupCount(sim->height), 1);
}

void InitSim(FluidSim *sim, int width, int height) {
  sim->width = width;
  sim->height = height;
  sim->velocityPing = 0;
  sim->densityPing = 0;
  sim->enableWindTunnel = true;
  sim->buoyancyStrength = 8.0f;
  sim->windSpeed = 2857.0f;  // ~250 km/h default
  sim->pressureVCycles = 2;
  sim->trackDisplayStats = true;
  sim->vorticityStrength = 5.0f;
  sim->smokeLineCount = 20;
  sim->smokeLineHalfWidth = 4.0f;

  // Initialize Smooth Stats
  sim->maxPressureSmooth = 1.0f;
  sim->maxVelocitySmooth = 1.0f;
  sim->maxCurlSmooth = 1.0f;

  // Create Textures
  sim->texDensity[0] = CreateTexture2D(width, height, GL_RGBA32F);
  sim->texDensity[1] = CreateTexture2D(width, height, GL_RGBA32F);
  sim->texVelocity[0] = CreateTexture2D(width, height, GL_RGBA32F);
  sim->texVelocity[1] = CreateTexture2D(width, height, GL_RGBA32F);
  sim->texPressure = CreateTexture2D(width, height, GL_R32F);
  sim->texDivergence = CreateTexture2D(width, height, GL_R32F);
  sim->texCurl = CreateTexture2D(width, height, GL_R32F);
  sim->texObstacles = CreateTexture2D(width, height, GL_R32F);

  // Load Shaders
  sim->shdAdvect =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_advect.glsl"));
  sim->shdDivergence =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_div.glsl"));
  sim->shdSubtract =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_sub.glsl"));
  sim->shdCurl =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_curl.glsl"));
  sim->shdVorticity =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_vort.glsl"));
  sim->shdSplat =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_splat.glsl"));
  sim->shdInlet =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_inlet.glsl"));
  sim->shdPaint =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_paint.glsl"));

  // Analysis Shaders
  sim->shdForce =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_force.glsl"));
  sim->shdAnalyze =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_analyze.glsl"));

  // Init Stats SSBO
  glGenBuffers(1, &sim->ssboStats);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboStats);
  glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(unsigned int), NULL,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Init Force SSBO
  glGenBuffers(1, &sim->ssboForce);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
  glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(int), NULL,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  sim->pressure = InitPressureSolver(sim->texPressure, sim->texDivergence,
                                     sim->texObstacles);
  ResetSim(sim, SCENE_CAR_WIND_TUNNEL);
}

static void RasterizeCarMask(float *oData, int width, int height) {
  // Load car silhouette from image.
  // The image is black car on white background, already flipped to face left.
  // We threshold: dark pixels (< 128) = solid obstacle.
  // The image is scaled to fit the simulation grid, vertically centered,
  // and positioned so the car occupies the left ~60% of the domain.
  Image carImg = LoadImage("resources/car_mask.png");
  if (carImg.data != NULL) {
    ImageFormat(&carImg, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

    // Target region in simulation space — preserve the image's aspect ratio.
    float aspect = (float)carImg.width / (float)carImg.height;
    int carW = (int)(width * 0.62f);        // car spans 62% of grid width
    int carH = (int)(carW / aspect);         // height derived from aspect ratio
    int carX = (int)(width * 0.05f);        // left margin
    int carY = (height - carH) / 2;          // vertically centered

    ImageResize(&carImg, carW, carH);

    unsigned char *pixels = (unsigned char *)carImg.data;
    for (int iy = 0; iy < carH; iy++) {
      for (int ix = 0; ix < carW; ix++) {
        unsigned char grey = pixels[iy * carW + ix];
        if (grey < 128) {
          // Texture y=0 is bottom of screen; image y=0 is top — flip vertically
          int tx = carX + ix;
          int ty = carY + (carH - 1 - iy);
          if (tx >= 0 && tx < width && ty >= 0 && ty < height)
            oData[ty * width + tx] = 1.0f;
        }
      }
    }
    UnloadImage(carImg);
  } else {
    TraceLog(LOG_WARNING, "Could not load resources/car_mask.png");
  }
}

static void RasterizeCircle(float *oData, int width, int height,
                            Vector2 center, float radius) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      float dx = (float)x - center.x;
      float dy = (float)y - center.y;
      if (dx * dx + dy * dy < radius * radius)
        oData[y * width + x] = 1.0f;
    }
  }
}

static bool IsSolidMaskPixel(Color c) {
  int luminance = (c.r + c.g + c.b) / 3;
  return c.a >= 128 && luminance < 128;
}

/** Nearest-neighbour scales the mask into `shape`'s box; row 0 is its top. */
static void RasterizeMask(float *oData, int width, int height,
                          const Color *pixels, int maskWidth, int maskHeight,
                          ObstacleShape shape) {
  float boxHeight = shape.size * (float)height;
  float boxWidth = boxHeight * (float)maskWidth / (float)maskHeight;
  float left = shape.centerX * (float)width - boxWidth * 0.5f;
  float bottom = shape.centerY * (float)height - boxHeight * 0.5f;

  int x0 = (int)fmaxf(floorf(left), 0.0f);
  int x1 = (int)fminf(ceilf(left + boxWidth), (float)width);
  int y0 = (int)fmaxf(floorf(bottom), 0.0f);
  int y1 = (int)fminf(ceilf(bottom + boxHeight), (float)height);
  for (int y = y0; y < y1; y++) {
    float v = ((float)y + 0.5f - bottom) / boxHeight;
    if (v < 0.0f || v >= 1.0f)
      continue;
    int maskRow = maskHeight - 1 - (int)(v * (float)maskHeight);
    for (int x = x0; x < x1; x++) {
      float u = ((float)x + 0.5f - left) / boxWidth;
      if (u < 0.0f || u >= 1.0f)
        continue;
      int maskCol = (int)(u * (float)maskWidth);
      if (IsSolidMaskPixel(pixels[maskRow * maskWidth + maskCol]))
        oData[(size_t)y * width + x] = 1.0f;
    }
  }
}

static bool RasterizeMaskFile(float *oData, int width, int height,
                              ObstacleShape shape) {
  Image image = LoadImage(shape.maskPath);
  if (image.data == NULL) {
    TraceLog(LOG_ERROR, "Could not load obstacle mask %s", shape.maskPath);
    return false;
  }
  Color *pixels = LoadImageColors(image);
  RasterizeMask(oData, width, height, pixels, image.width, image.height,
                shape);
  UnloadImageColors(pixels);
  UnloadImage(image);
  return true;
}

static void UploadObstacles(FluidSim *sim, const float *oData) {
  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RED,
                  GL_FLOAT, oData);
}

static void UploadCarObstacle(FluidSim *sim) {
  float *oData =
      (float *)calloc((size_t)sim->width * sim->height, sizeof(float));
  if (oData == NULL) {
    TraceLog(LOG_WARNING, "Could not allocate the car obstacle");
    return;
  }
  RasterizeCarMask(oData, sim->width, sim->height);
  UploadObstacles(sim, oData);
  free(oData);
}

static bool UploadShapeObstacle(FluidSim *sim, const ObstacleShape *shape) {
  float *oData =
      (float *)calloc((size_t)sim->width * sim->height, sizeof(float));
  if (oData == NULL)
    return false;

  bool ok = true;
  if (shape->maskPath == NULL)
    RasterizeCircle(oData, sim->width, sim->height,
                    (Vector2){shape->centerX * (float)sim->width,
                              shape->centerY * (float)sim->height},
                    shape->size * 0.5f * (float)sim->height);
  else
    ok = RasterizeMaskFile(oData, sim->width, sim->height, *shape);

  if (ok)
    UploadObstacles(sim, oData);
  free(oData);
  return ok;
}

void ResetSim(FluidSim *sim, SimScene scene) {
  // Reset Smooth Stats
  sim->maxPressureSmooth = 0.1f;
  sim->maxVelocitySmooth = 0.1f;

  float *zeroData =
      (float *)calloc((size_t)sim->width * sim->height * 4, sizeof(float));

  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texDensity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texDensity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texPressure.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RED, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim->width, sim->height, GL_RED, GL_FLOAT,
                  zeroData);

  free(zeroData);

  sim->buoyancyStrength = scene == SCENE_FREE ? 8.0f : 0.0f;
  if (scene == SCENE_CAR_WIND_TUNNEL)
    UploadCarObstacle(sim);
  if (scene == SCENE_CIRCLE_WIND_TUNNEL) {
    ObstacleShape circle = DefaultObstacleShape();
    if (!UploadShapeObstacle(sim, &circle))
      TraceLog(LOG_WARNING, "Could not place the circle obstacle");
  }
  RebuildPressureMasks(&sim->pressure);
}

bool PlaceObstacle(FluidSim *sim, const ObstacleShape *shape) {
  if (!UploadShapeObstacle(sim, shape))
    return false;
  RebuildPressureMasks(&sim->pressure);
  return true;
}

void ApplySplat(FluidSim *sim, Texture2D_GL tex, Vector2 pos, float radius,
                Vector4 color) {
  rlEnableShader(sim->shdSplat);
  glBindImageTexture(0, tex.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "color"), &color,
               RL_SHADER_UNIFORM_VEC4, 1);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

void PaintObstacle(FluidSim *sim, Vector2 pos, float radius, bool erase) {
  rlEnableShader(sim->shdPaint);
  glBindImageTexture(0, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_R32F);
  float val = erase ? 0.0f : 1.0f;
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "value"), &val,
               RL_SHADER_UNIFORM_FLOAT, 1);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
  RebuildPressureMasks(&sim->pressure);
}

static void UpdateDisplayStats(FluidSim *sim) {
  Vector2 res = {(float)sim->width, (float)sim->height};
  unsigned int zeroStats[3] = {0, 0, 0};
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboStats);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zeroStats), zeroStats);

  rlEnableShader(sim->shdAnalyze);
  glBindImageTexture(0, sim->texPressure.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindImageTexture(1, sim->texVelocity[sim->velocityPing].id, 0, GL_FALSE, 0,
                     GL_READ_ONLY, GL_RGBA32F);
  glBindImageTexture(2, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sim->ssboStats);
  rlSetUniform(rlGetLocationUniform(sim->shdAnalyze, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  rlDisableShader();

  unsigned int readStats[3];
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(readStats), readStats);

  float rawMaxP = 0.0f;
  float rawMaxV = 0.0f;
  float rawMaxC = 0.0f;
  memcpy(&rawMaxP, &readStats[0], sizeof(float));
  memcpy(&rawMaxV, &readStats[1], sizeof(float));
  memcpy(&rawMaxC, &readStats[2], sizeof(float));

  if (rawMaxP < 0.0001f) rawMaxP = 0.0001f;
  if (rawMaxV < 0.0001f) rawMaxV = 0.0001f;
  if (rawMaxC < 0.0001f) rawMaxC = 0.0001f;

  sim->maxPressureSmooth = sim->maxPressureSmooth * 0.95f + rawMaxP * 0.05f;
  sim->maxVelocitySmooth = sim->maxVelocitySmooth * 0.95f + rawMaxV * 0.05f;
  sim->maxCurlSmooth     = sim->maxCurlSmooth     * 0.95f + rawMaxC * 0.05f;
}

static void DispatchAerodynamicForces(FluidSim *sim) {
  Vector2 res = {(float)sim->width, (float)sim->height};
  int zero[2] = {0, 0};
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), zero);
  rlEnableShader(sim->shdForce);
  glBindImageTexture(0, sim->texPressure.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssboForce);
  rlSetUniform(rlGetLocationUniform(sim->shdForce, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  rlDisableShader();
}

/** Semi-Lagrangian advection of `src` into `dst` by the current velocity. */
static void Advect(FluidSim *sim, Texture2D_GL src, Texture2D_GL dst,
                   float dt) {
  Vector2 res = {(float)sim->width, (float)sim->height};
  rlEnableShader(sim->shdAdvect);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[sim->velocityPing].id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, src.id);
  glBindImageTexture(3, dst.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

static void AdvectDensity(FluidSim *sim, float dt) {
  int next = !sim->densityPing;
  Advect(sim, sim->texDensity[sim->densityPing], sim->texDensity[next], dt);
  sim->densityPing = next;
}

static void StepVelocity(FluidSim *sim, float dt, float time) {
  int next = !sim->velocityPing;
  Advect(sim, sim->texVelocity[sim->velocityPing], sim->texVelocity[next], dt);
  sim->velocityPing = next;
  int p = sim->velocityPing;

  // 2. Inlet
  if (sim->enableWindTunnel) {
    rlEnableShader(sim->shdInlet);
    glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA32F);
    glBindImageTexture(1, sim->texDensity[sim->densityPing].id, 0, GL_FALSE, 0,
                       GL_WRITE_ONLY, GL_RGBA32F);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "time"), &time,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "windSpeed"), &sim->windSpeed,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "lineCount"),
                 &sim->smokeLineCount, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "lineHalfWidth"),
                 &sim->smokeLineHalfWidth, RL_SHADER_UNIFORM_FLOAT, 1);
    rlComputeShaderDispatch(2, GroupCount(sim->height), 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    rlDisableShader();
  }

  // 3. Curl
  rlEnableShader(sim->shdCurl);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA32F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R32F);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 4. Vorticity
  rlEnableShader(sim->shdVorticity);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA32F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "curlStrength"),
               &sim->vorticityStrength, RL_SHADER_UNIFORM_FLOAT, 1);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 5. Divergence
  rlEnableShader(sim->shdDivergence);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA32F);
  glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindImageTexture(2, sim->texDivergence.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R32F);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 6. Pressure
  SolvePressure(&sim->pressure, sim->pressureVCycles);

  // 7. Subtract
  rlEnableShader(sim->shdSubtract);
  glBindImageTexture(0, sim->texPressure.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA32F);
  glBindImageTexture(2, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R32F);
  DispatchOverGrid(sim);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();

  if (sim->trackDisplayStats) {
    UpdateDisplayStats(sim);
    if (sim->enableWindTunnel)
      DispatchAerodynamicForces(sim);
  }
}

void UpdateSimSubstepped(FluidSim *sim, float dt, float time,
                         int velocitySubsteps) {
  AdvectDensity(sim, dt);
  float subDt = dt / (float)velocitySubsteps;
  for (int i = 0; i < velocitySubsteps; i++)
    StepVelocity(sim, subDt, time - dt + subDt * (float)(i + 1));
}

void UpdateSim(FluidSim *sim, float dt, float time) {
  UpdateSimSubstepped(sim, dt, time, 1);
}

Vector2 GetAerodynamicForces(FluidSim *sim) {
  if (!sim->enableWindTunnel)
    return (Vector2){0, 0};
  int data[2];
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data);
  float rawDrag = (float)data[0] / 100.0f;
  float rawLift = (float)data[1] / 100.0f;
  float visualScale = 0.005f;
  return (Vector2){rawDrag * visualScale, rawLift * visualScale};
}
