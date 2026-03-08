#include "simulation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy

void InitSim(FluidSim *sim) {
  sim->ping = 0;
  sim->enableWindTunnel = true;
  sim->buoyancyStrength = 8.0f;
  sim->windSpeed = 2857.0f;  // ~250 km/h default

  // Initialize Smooth Stats
  sim->maxPressureSmooth = 1.0f;
  sim->maxVelocitySmooth = 1.0f;
  sim->maxCurlSmooth = 1.0f;

  // Create Textures
  sim->texDensity[0] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texDensity[1] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texVelocity[0] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texVelocity[1] = CreateTexture2D(RES_X, RES_Y, GL_RGBA16F);
  sim->texPressure[0] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texPressure[1] = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texDivergence = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texCurl = CreateTexture2D(RES_X, RES_Y, GL_R16F);
  sim->texObstacles = CreateTexture2D(RES_X, RES_Y, GL_R16F);

  // Load Shaders
  sim->shdAdvect =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_advect.glsl"));
  sim->shdDivergence =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_div.glsl"));
  sim->shdJacobi =
      LoadCompute(LoadFileText("resources/shaders/fluid2d_jacobi.glsl"));
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

  ResetSim(sim, 1);
}

void ResetSim(FluidSim *sim, int mode) {
  // Reset Smooth Stats
  sim->maxPressureSmooth = 0.1f;
  sim->maxVelocitySmooth = 0.1f;

  float *zeroData = (float *)calloc(RES_X * RES_Y * 4, sizeof(float));

  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texDensity[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texDensity[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RGBA, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texPressure[0].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);
  glBindTexture(GL_TEXTURE_2D, sim->texPressure[1].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);

  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                  zeroData);

  free(zeroData);

  if (mode == 1) {
    sim->buoyancyStrength = 0.0f;
    float *oData = (float *)calloc(RES_X * RES_Y, sizeof(float));

    // F1 car 2D side-view silhouette
    // Coordinate system: texture y=0 is BOTTOM of screen (OpenGL convention).
    // The texture is drawn flipped (DrawTexturePro uses -RES_Y source height).
    // So: LARGER y = HIGHER on screen.  oy is the car centerline.
    //
    // Wind flows left→right. Car nose points LEFT (into wind).
    float ox = RES_X * 0.20f;   // nose tip x
    float oy = RES_Y * 0.50f;   // car centerline y
    float S  = RES_X / 1280.0f; // pixel scale

    // ---- X landmarks (nose=left, rear=right) ----
    float xNoseTip  = ox;
    float xNoseRoot = ox + 90*S;
    float xBodyF    = ox + 90*S;
    float xCockF    = ox + 110*S;
    float xCockPeak = ox + 170*S;
    float xCockR    = ox + 240*S;
    float xSidepodF = ox + 120*S;
    float xSidepodR = ox + 310*S;
    float xBodyR    = ox + 350*S;
    float xDiffR    = ox + 395*S;

    // ---- Y landmarks (larger y = higher on screen) ----
    float yBot      = oy - 20*S;   // underfloor
    float yTop      = oy + 20*S;   // top of chassis slab
    float yNoseB    = oy -  7*S;   // nose bottom (thin tip)
    float yNoseT    = oy +  7*S;   // nose top
    float yCockPeak = oy + 55*S;   // cockpit peak (rises above chassis)
    float ySidepodT = oy + 30*S;   // sidepod top
    float ySidepodB = oy - 28*S;   // sidepod undercut
    float yDiffExit = oy - 52*S;   // diffuser exit

    // ---- Wheels (sit below chassis floor, tangent to it) ----
    float wR        = 24*S;
    float xFWheelC  = ox + 58*S;
    float xRWheelC  = ox + 320*S;
    float yWheelC   = oy - 20*S - wR;  // tangent to floor

    // ---- Front wing ----
    // Sits below the nose, connected via a vertical strut at the trailing edge
    float xFWingLE  = ox - 50*S;  // leading edge (ahead of nose tip)
    float xFWingTE  = ox + 25*S;  // trailing edge (under nose)
    float yFWingT   = oy - 22*S;  // top surface — at car floor level so strut connects
    float yFWingB   = oy - 30*S;  // bottom surface (thin element)
    float yFWingEP  = oy - 50*S;  // endplate bottom

    // ---- Rear wing ----
    // Low-profile: sits just above the car body, connected by a wide pylon
    float xRWingLE  = ox + 350*S;
    float xRWingTE  = ox + 392*S;
    float yRWingT   = oy + 65*S;  // top of main element
    float yRWingB   = oy + 52*S;  // bottom of main element
    float yRWing2T  = oy + 48*S;  // second flap top
    float yRWing2B  = oy + 40*S;  // second flap bottom
    float yRWingEP  = oy + 10*S;  // endplate bottom (embedded in car body)

    for (int y = 0; y < RES_Y; y++) {
      for (int x = 0; x < RES_X; x++) {
        float fx = (float)x;
        float fy = (float)y;
        bool solid = false;

        // --- NOSE CONE (tapered slab, thin at tip) ---
        if (fx >= xNoseTip && fx < xNoseRoot) {
          float t = (fx - xNoseTip) / (xNoseRoot - xNoseTip);
          float ts = t * t * (3.0f - 2.0f * t); // smooth step
          float lo = yNoseB + ts * (yBot  - yNoseB);
          float hi = yNoseT + ts * (yTop  - yNoseT);
          if (fy >= lo && fy <= hi) solid = true;
        }

        // --- MAIN CHASSIS SLAB ---
        if (fx >= xBodyF && fx < xSidepodR) {
          if (fy >= yBot && fy <= yTop) solid = true;
        }

        // --- SIDEPOD BULGE (wider mid-section) ---
        if (fx >= xSidepodF && fx < xSidepodR) {
          float ramp = 40*S;
          float t = 1.0f;
          if (fx < xSidepodF + ramp)      t = (fx - xSidepodF) / ramp;
          else if (fx > xSidepodR - ramp) t = (xSidepodR - fx) / ramp;
          float lo = yBot  + (ySidepodB - yBot)  * t;  // widens downward
          float hi = yTop  + (ySidepodT - yTop)  * t;  // widens upward
          if (fy >= lo && fy <= hi) solid = true;
        }

        // --- COCKPIT FAIRING (raised hump above chassis) ---
        if (fx >= xCockF && fx < xCockR) {
          float t;
          if (fx < xCockPeak)
            t = (fx - xCockF) / (xCockPeak - xCockF);
          else
            t = 1.0f - (fx - xCockPeak) / (xCockR - xCockPeak);
          t = t * t * (3.0f - 2.0f * t);
          float hi = yTop + (yCockPeak - yTop) * t; // rises above yTop
          if (fy >= yBot && fy <= hi) solid = true;
        }

        // --- REAR TAPER ---
        if (fx >= xSidepodR && fx < xBodyR) {
          float t = (fx - xSidepodR) / (xBodyR - xSidepodR);
          float lo = yBot + t * 8*S;   // floor rises
          float hi = yTop - t * 8*S;   // roof drops
          if (fy >= lo && fy <= hi) solid = true;
        }

        // --- DIFFUSER (angled ramp, floor drops at rear) ---
        if (fx >= xBodyR && fx < xDiffR) {
          float t = (fx - xBodyR) / (xDiffR - xBodyR);
          float floorY = yBot + t * (yDiffExit - yBot); // floor descends
          float roofY  = yTop - t * 10*S;
          if (fy >= floorY && fy <= roofY) solid = true;
        }

        // --- FRONT WHEELS ---
        {
          float dx = fx - xFWheelC, dy = fy - yWheelC;
          if (dx*dx + dy*dy <= wR*wR) solid = true;
        }

        // --- REAR WHEELS ---
        {
          float dx = fx - xRWheelC, dy = fy - yWheelC;
          if (dx*dx + dy*dy <= wR*wR) solid = true;
        }

        // --- FRONT WING: main plane (slight camber, leading edge lower) ---
        if (fx >= xFWingLE && fx < xFWingTE) {
          float t = (fx - xFWingLE) / (xFWingTE - xFWingLE);
          float camber = (1.0f - t) * 6*S;
          float lo = yFWingB - camber;
          float hi = yFWingT - camber;
          if (fy >= lo && fy <= hi) solid = true;
        }

        // --- FRONT WING: vertical strut connecting wing to nose underside ---
        // At the trailing edge, a strut rises from wing top up to the nose floor
        if (fx >= xFWingTE - 4*S && fx < xFWingTE + 4*S) {
          // nose floor at this x: interpolate nose bottom
          float t  = (xFWingTE - xNoseTip) / (xNoseRoot - xNoseTip);
          float ts = t * t * (3.0f - 2.0f * t);
          float noseFloor = yNoseB + ts * (yBot - yNoseB);
          if (fy >= yFWingT && fy <= noseFloor) solid = true;
        }

        // --- FRONT WING ENDPLATES (vertical, at leading edge only) ---
        if (fx >= xFWingLE && fx < xFWingLE + 5*S)
          if (fy >= yFWingEP && fy <= yFWingT) solid = true;

        // --- REAR WING: main element ---
        if (fx >= xRWingLE && fx < xRWingTE)
          if (fy >= yRWingB && fy <= yRWingT) solid = true;

        // --- REAR WING: second flap element ---
        if (fx >= xRWingLE + 4*S && fx < xRWingTE - 4*S)
          if (fy >= yRWing2B && fy <= yRWing2T) solid = true;

        // --- REAR WING ENDPLATES (wide enough to look substantial) ---
        if (fx >= xRWingLE && fx < xRWingLE + 8*S)
          if (fy >= yRWingEP && fy <= yRWingT) solid = true;
        if (fx >= xRWingTE - 8*S && fx < xRWingTE)
          if (fy >= yRWingEP && fy <= yRWingT) solid = true;

        // --- REAR WING PYLON (wide strut connecting body to wing underside) ---
        {
          float pylonX = (xRWingLE + xRWingTE) * 0.5f;
          if (fabsf(fx - pylonX) < 8*S)
            if (fy >= yRWingEP && fy <= yRWing2B) solid = true;
        }

        // --- WHEEL AXLE STUBS (connect wheels to underfloor) ---
        if (fabsf(fx - xFWheelC) < 8*S)
          if (fy >= yWheelC && fy <= yBot + 2*S) solid = true;
        if (fabsf(fx - xRWheelC) < 8*S)
          if (fy >= yWheelC && fy <= yBot + 2*S) solid = true;

        if (solid)
          oData[y * RES_X + x] = 1.0f;
      }
    }
    glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RES_X, RES_Y, GL_RED, GL_FLOAT,
                    oData);
    free(oData);
  } else {
    sim->buoyancyStrength = 8.0f;
  }
}

void ApplySplat(FluidSim *sim, Texture2D_GL tex, Vector2 pos, float radius,
                Vector4 color) {
  rlEnableShader(sim->shdSplat);
  glBindImageTexture(0, tex.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdSplat, "color"), &color,
               RL_SHADER_UNIFORM_VEC4, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

void PaintObstacle(FluidSim *sim, Vector2 pos, float radius, bool erase) {
  rlEnableShader(sim->shdPaint);
  glBindImageTexture(0, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_R16F);
  float val = erase ? 0.0f : 1.0f;
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "point"), &pos,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "radius"), &radius,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdPaint, "value"), &val,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();
}

void UpdateSim(FluidSim *sim, float dt, float time) {
  int p = sim->ping;
  int next_p = !p;
  Vector2 res = {(float)RES_X, (float)RES_Y};

  // 1. Advect
  rlEnableShader(sim->shdAdvect);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdAdvect, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[p].id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, sim->texObstacles.id);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, sim->texVelocity[p].id);
  glBindImageTexture(3, sim->texVelocity[next_p].id, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, sim->texDensity[p].id);
  glBindImageTexture(3, sim->texDensity[next_p].id, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  sim->ping = next_p;
  p = sim->ping;

  // 2. Inlet
  if (sim->enableWindTunnel) {
    rlEnableShader(sim->shdInlet);
    glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    glBindImageTexture(1, sim->texDensity[p].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "time"), &time,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(sim->shdInlet, "windSpeed"), &sim->windSpeed,
                 RL_SHADER_UNIFORM_FLOAT, 1);
    rlComputeShaderDispatch(2, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    rlDisableShader();
  }

  // 3. Curl
  rlEnableShader(sim->shdCurl);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 4. Vorticity
  rlEnableShader(sim->shdVorticity);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
  float curlStr = 5.0f;
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(sim->shdVorticity, "curlStrength"),
               &curlStr, RL_SHADER_UNIFORM_FLOAT, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 5. Divergence
  rlEnableShader(sim->shdDivergence);
  glBindImageTexture(0, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(2, sim->texDivergence.id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // 6. Jacobi
  rlEnableShader(sim->shdJacobi);
  for (int i = 0; i < 40; i++) {
    glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, sim->texDivergence.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(2, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(3, sim->texPressure[1].id, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_R16F);
    rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    Texture2D_GL tmp = sim->texPressure[0];
    sim->texPressure[0] = sim->texPressure[1];
    sim->texPressure[1] = tmp;
  }

  // 7. Subtract
  rlEnableShader(sim->shdSubtract);
  glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_WRITE,
                     GL_RGBA16F);
  glBindImageTexture(2, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  rlDisableShader();

  // --- AUTO-RANGE ANALYSIS ---
  unsigned int zeroStats[3] = {0, 0, 0};
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboStats);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zeroStats), zeroStats);

  rlEnableShader(sim->shdAnalyze);
  glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindImageTexture(1, sim->texVelocity[p].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_RGBA16F);
  glBindImageTexture(2, sim->texCurl.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                     GL_R16F);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sim->ssboStats);
  rlSetUniform(rlGetLocationUniform(sim->shdAnalyze, "res"), &res,
               RL_SHADER_UNIFORM_VEC2, 1);
  rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
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

  // --- Force Calculation ---
  if (sim->enableWindTunnel) {
    int zero[2] = {0, 0};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssboForce);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), zero);
    rlEnableShader(sim->shdForce);
    glBindImageTexture(0, sim->texPressure[0].id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindImageTexture(1, sim->texObstacles.id, 0, GL_FALSE, 0, GL_READ_ONLY,
                       GL_R16F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssboForce);
    rlSetUniform(rlGetLocationUniform(sim->shdForce, "res"), &res,
                 RL_SHADER_UNIFORM_VEC2, 1);
    rlComputeShaderDispatch((RES_X + 15) / 16, (RES_Y + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    rlDisableShader();
  }
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
