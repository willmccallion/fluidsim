#include "particles.h"
#include "raylib.h"
#include "rlgl.h" // Needed for rlActiveTextureSlot
#include "simulation.h"
#include <math.h>
#include <stdio.h>

// --- OpenGL Compatibility Definitions ---
// Ensure GL_TEXTURE_2D is available
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif

// Ensure glBindTexture is declared if standard headers are missing on some
// platforms
#ifndef __linux__
extern void glBindTexture(unsigned int target, unsigned int texture);
#endif

int main() {
  // 1. Setup Window
  InitWindow(1280, 640, "2D Fluid: Real-Time CFD Engine");
  SetTargetFPS(60);

  // 2. Initialize Systems
  FluidSim sim;
  InitSim(&sim);

  ParticleSys particles;
  InitParticles(&particles);

  Shader shdDisplay  = LoadShader(0, "resources/shaders/display.fs");
  Shader shdObstacle = LoadShader(0, "resources/shaders/obstacle.fs");

  // 3. State Variables
  // 0 = Standard (RGB), 1 = Pressure Tint, 2 = Velocity Tint, 3 = Curl
  int viewMode = 0;
  bool showParticles = true;
  bool showHUD = true;
  float time = 0.0f;
  float brushRadius = 65.0f;

// Graphing Data
#define GRAPH_WIDTH 300
  float dragHistory[GRAPH_WIDTH] = {0};
  int graphIdx = 0;
  float smoothedDrag = 0.0f;

  // --- MAIN LOOP ---
  while (!WindowShouldClose()) {
    float dt = 0.005f;
    time += dt;

    // --- INPUT HANDLING ---

    // Toggle Wind Tunnel
    if (IsKeyPressed(KEY_W))
      sim.enableWindTunnel = !sim.enableWindTunnel;

    // Toggle Particles
    if (IsKeyPressed(KEY_SPACE))
      showParticles = !showParticles;

    // Cycle Visualization Modes (V key)
    if (IsKeyPressed(KEY_V))
      viewMode = (viewMode + 1) % 4;

    // Toggle HUD
    if (IsKeyPressed(KEY_H))
      showHUD = !showHUD;

    // Reset Modes
    if (IsKeyPressed(KEY_ONE)) {
      ResetSim(&sim, 0);
      sim.enableWindTunnel = false;
    }
    if (IsKeyPressed(KEY_TWO)) {
      ResetSim(&sim, 1);
      sim.enableWindTunnel = true;
    }

    // Wind Speed (F/G keys) — internal range 571–4000 maps to 50–350 km/h
    // Step of ~114 internal units = ~10 km/h
    if (IsKeyDown(KEY_F))
      sim.windSpeed = fmaxf(571.0f, sim.windSpeed - 114.0f);
    if (IsKeyDown(KEY_G))
      sim.windSpeed = fminf(4000.0f, sim.windSpeed + 114.0f);

    // Brush Size (Scroll Wheel)
    float scroll = GetMouseWheelMove();
    if (scroll != 0.0f)
      brushRadius = fmaxf(10.0f, fminf(300.0f, brushRadius + scroll * 8.0f));

    // Mouse Interaction
    Vector2 mPos = GetMousePosition();
    Vector2 mDelta = GetMouseDelta();
    float scaleX = (float)RES_X / GetScreenWidth();
    float scaleY = (float)RES_Y / GetScreenHeight();
    Vector2 simPos = {mPos.x * scaleX, (GetScreenHeight() - mPos.y) * scaleY};

    // Left Click: Add Dye & Velocity
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      Vector4 velAdd = {mDelta.x * 20.0f * scaleX, -mDelta.y * 20.0f * scaleY,
                        0.0f, 0.0f};
      ApplySplat(&sim, sim.texVelocity[sim.ping], simPos, brushRadius, velAdd);

      // Rainbow Dye
      Color c = ColorFromHSV((float)GetTime() * 100.0f, 0.7f, 0.9f);
      Vector4 colAdd = {(c.r / 255.0f) * 4.0f, (c.g / 255.0f) * 4.0f,
                        (c.b / 255.0f) * 4.0f, 1.0f};
      ApplySplat(&sim, sim.texDensity[sim.ping], simPos, brushRadius, colAdd);
    }

    // Right Click: Draw Wall
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      PaintObstacle(&sim, simPos, brushRadius * 0.4f, false);
    }

    // Middle Click: Erase Wall
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
      PaintObstacle(&sim, simPos, brushRadius * 0.6f, true);
    }

    // --- SIMULATION STEP ---
    for (int step = 0; step < 2; step++) {
      UpdateSim(&sim, dt, time);

      // Auto-spawn particles if in Wind Tunnel mode
      if (sim.enableWindTunnel)
        SeedWindTunnelParticles(&particles, dt);

      UpdateParticles(&particles, &sim, dt, time);
    }

    // --- DATA ANALYSIS ---
    Vector2 forces = GetAerodynamicForces(&sim);
    // Low-pass filter for smooth graph
    smoothedDrag = smoothedDrag * 0.90f + forces.x * 0.10f;
    dragHistory[graphIdx] = smoothedDrag;
    graphIdx = (graphIdx + 1) % GRAPH_WIDTH;

    // --- RENDERING ---
    BeginDrawing();
    ClearBackground(BLACK);

    // 1. Draw Fluid (The Complex Part)
    BeginShaderMode(shdDisplay);

    // Pass View Mode
    SetShaderValue(shdDisplay, GetShaderLocation(shdDisplay, "mode"), &viewMode,
                   SHADER_UNIFORM_INT);

    // Pass Auto-Range Stats (Normalization)
    SetShaderValue(shdDisplay, GetShaderLocation(shdDisplay, "maxPressure"),
                   &sim.maxPressureSmooth, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shdDisplay, GetShaderLocation(shdDisplay, "maxVelocity"),
                   &sim.maxVelocitySmooth, SHADER_UNIFORM_FLOAT);

    // Pass maxCurl uniform
    SetShaderValue(shdDisplay, GetShaderLocation(shdDisplay, "maxCurl"),
                   &sim.maxCurlSmooth, SHADER_UNIFORM_FLOAT);

    // Bind Secondary Texture (Data) to Slot 1
    rlActiveTextureSlot(1);
    if (viewMode == 1)
      glBindTexture(GL_TEXTURE_2D, sim.texPressure[0].id);
    else if (viewMode == 2)
      glBindTexture(GL_TEXTURE_2D, sim.texVelocity[sim.ping].id);
    else if (viewMode == 3)
      glBindTexture(GL_TEXTURE_2D, sim.texCurl.id);
    else
      glBindTexture(GL_TEXTURE_2D, 0);

    // Reset to Slot 0 for the main drawing
    rlActiveTextureSlot(0);

    // Tell shader that "texture1" is in Slot 1
    int slot1 = 1;
    SetShaderValue(shdDisplay, GetShaderLocation(shdDisplay, "texture1"),
                   &slot1, SHADER_UNIFORM_INT);

    // Draw the Main Texture (Density)
    Texture2D raylibTex = {0};
    raylibTex.width = RES_X;
    raylibTex.height = RES_Y;
    raylibTex.mipmaps = 1;
    raylibTex.id = sim.texDensity[sim.ping].id;
    raylibTex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    SetTextureFilter(raylibTex, TEXTURE_FILTER_BILINEAR);
    DrawTexturePro(raylibTex, (Rectangle){0, 0, (float)RES_X, (float)-RES_Y},
                   (Rectangle){0, 0, 1280, 640}, (Vector2){0, 0}, 0.0f, WHITE);
    EndShaderMode();

    // 2. Draw Particles
    if (showParticles) {
      DrawParticles(&particles);
    }

    // 3. Draw Car — obstacle tex is R16F (red channel only).
    // Raylib tints by multiplying the color uniform with the texture sample.
    // R16F maps to red channel, so: draw with RED tint gives red*mask.
    // We use the obstacle shader which reads .r and outputs proper RGBA.
    // Fallback: additive grey (R+G+B separately) if shader binding issues.
    {
      Texture2D obsTex = {0};
      obsTex.id = sim.texObstacles.id;
      obsTex.width = RES_X;
      obsTex.height = RES_Y;
      obsTex.format = PIXELFORMAT_UNCOMPRESSED_R32;
      obsTex.mipmaps = 1;
      BeginShaderMode(shdObstacle);
      DrawTexturePro(obsTex, (Rectangle){0, 0, (float)RES_X, (float)-RES_Y},
                     (Rectangle){0, 0, 1280, 640}, (Vector2){0, 0}, 0.0f, WHITE);
      EndShaderMode();
    }

    // 4. UI & HUD
    if (showHUD) {
      int sw = GetScreenWidth();
      int sh = GetScreenHeight();

      // --- Colour palette ---
      Color cPanel   = (Color){ 8,  10,  16, 200};
      Color cBorder  = (Color){255, 255, 255,  18};
      Color cLabel   = (Color){120, 130, 150, 255};
      Color cWhite   = (Color){230, 235, 245, 255};
      Color cAccent  = (Color){ 30, 160, 255, 255};   // blue accent
      Color cDrag    = (Color){255,  80,  60, 255};   // orange-red
      Color cLiftPos = (Color){ 30, 200, 100, 255};   // downforce (good)
      Color cLiftNeg = (Color){255, 200,  40, 255};   // lift (bad)
      Color cBar     = (Color){ 20,  24,  34, 255};
      Color cBarFill = (Color){ 30, 160, 255, 180};

      // Helper macro: semi-transparent rounded panel
      float rad = 0.08f;

      // ================================================================
      // TOP-LEFT: Sim info panel
      // ================================================================
      int pX = 14, pY = 14, pW = 280, pH = 102;
      DrawRectangleRounded((Rectangle){pX, pY, pW, pH}, rad, 8, cPanel);
      DrawRectangleRoundedLines((Rectangle){pX, pY, pW, pH}, rad, 8, cBorder);

      // Title bar accent line
      DrawRectangle(pX + 1, pY + 1, pW - 2, 2, cAccent);

      // FPS  (top-right of panel)
      DrawText(TextFormat("%d fps", GetFPS()), pX + pW - 55, pY + 8, 11, (Color){60, 200, 100, 255});

      // View mode chip
      const char *modeName = "SMOKE";
      Color modeChipColor = (Color){50, 50, 60, 255};
      Color modeTextColor = cWhite;
      if (viewMode == 1) { modeName = "PRESSURE";  modeChipColor = (Color){20, 50, 90, 255};  modeTextColor = cAccent; }
      if (viewMode == 2) { modeName = "VELOCITY";  modeChipColor = (Color){70, 50, 10, 255};  modeTextColor = (Color){255, 200, 60, 255}; }
      if (viewMode == 3) { modeName = "VORTICITY"; modeChipColor = (Color){60, 10, 60, 255};  modeTextColor = (Color){200, 80, 255, 255}; }
      DrawRectangleRounded((Rectangle){pX + 10, pY + 10, 90, 18}, 0.5f, 4, modeChipColor);
      DrawText(modeName, pX + 14, pY + 13, 11, modeTextColor);

      // Divider
      DrawLine(pX + 10, pY + 34, pX + pW - 10, pY + 34, (Color){255,255,255,14});

      // BRUSH row
      float bNorm = (brushRadius - 10.0f) / 290.0f;
      DrawText("BRUSH", pX + 10, pY + 42, 10, cLabel);
      DrawRectangleRounded((Rectangle){pX + 56, pY + 42, 190, 8}, 0.5f, 4, cBar);
      DrawRectangleRounded((Rectangle){pX + 56, pY + 42, (int)(bNorm * 190), 8}, 0.5f, 4, (Color){100,160,255,200});
      DrawText(TextFormat("%3.0f", brushRadius), pX + 252, pY + 40, 10, cLabel);

      // WIND row — display in km/h (internal / 4000 * 350)
      float windKmh = sim.windSpeed * (350.0f / 4000.0f);
      float wNorm = (sim.windSpeed - 571.0f) / (4000.0f - 571.0f);
      Color windColor = sim.enableWindTunnel ? (Color){255, 160, 50, 255} : cLabel;
      Color windFill  = sim.enableWindTunnel ? (Color){255, 140, 30, 180} : (Color){60,60,60,150};
      DrawText("WIND ", pX + 10, pY + 60, 10, windColor);
      DrawRectangleRounded((Rectangle){pX + 56, pY + 60, 190, 8}, 0.5f, 4, cBar);
      DrawRectangleRounded((Rectangle){pX + 56, pY + 60, (int)(wNorm * 190), 8}, 0.5f, 4, windFill);
      DrawText(TextFormat("%3.0f km/h", windKmh), pX + 232, pY + 58, 10, cLabel);

      // Keybinds hint (very subtle)
      DrawText("[V] view  [F/G] wind ±10km/h  [Space] particles  [H] hide  [2] reset", pX + 10, pY + 84, 8, (Color){80, 85, 100, 200});

      // ================================================================
      // BOTTOM-LEFT: Aerodynamics telemetry panel (wind tunnel only)
      // ================================================================
      if (sim.enableWindTunnel) {
        int tX = 14, tH = 178, tW = 310, tY = sh - tH - 14;
        DrawRectangleRounded((Rectangle){tX, tY, tW, tH}, rad, 8, cPanel);
        DrawRectangleRoundedLines((Rectangle){tX, tY, tW, tH}, rad, 8, cBorder);

        // Top accent
        DrawRectangle(tX + 1, tY + 1, tW - 2, 2, cDrag);

        // Section label
        DrawText("AERODYNAMICS", tX + 12, tY + 10, 9, cLabel);

        // Drag readout
        DrawText("DRAG", tX + 12, tY + 28, 10, cLabel);
        DrawText(TextFormat("%.2f", smoothedDrag), tX + 52, tY + 24, 20, cDrag);

        // Lift readout
        bool isDownforce = forces.y < 0;
        Color liftCol = isDownforce ? cLiftPos : cLiftNeg;
        DrawText(isDownforce ? "DWF " : "LIFT", tX + 166, tY + 28, 10, cLabel);
        DrawText(TextFormat("%.2f", fabsf(forces.y)), tX + 206, tY + 24, 20, liftCol);

        // Divider
        DrawLine(tX + 10, tY + 52, tX + tW - 10, tY + 52, (Color){255,255,255,14});

        // Drag/Lift ratio
        float dl = (fabsf(forces.y) > 0.001f) ? (smoothedDrag / fabsf(forces.y)) : 0.0f;
        DrawText("L/D", tX + 12, tY + 60, 9, cLabel);
        DrawText(TextFormat("%.3f", dl), tX + 42, tY + 57, 14, cWhite);

        // Graph area
        int gX = tX + 10, gY = tY + tH - 12, gW = tW - 20, gH = 72;
        DrawRectangleRounded((Rectangle){gX, gY - gH, gW, gH}, 0.05f, 4, (Color){4, 6, 12, 220});
        DrawText("DRAG HISTORY", gX + 4, gY - gH + 4, 8, (Color){70, 80, 100, 200});

        // Horizontal center grid line
        DrawLine(gX, gY - gH/2, gX + gW, gY - gH/2, (Color){255,255,255, 8});

        float minVal = dragHistory[0], maxVal = dragHistory[0];
        for (int i = 0; i < GRAPH_WIDTH; i++) {
          if (dragHistory[i] < minVal) minVal = dragHistory[i];
          if (dragHistory[i] > maxVal) maxVal = dragHistory[i];
        }
        if (maxVal - minVal < 30.0f) {
          float center = (maxVal + minVal) * 0.5f;
          maxVal = center + 15.0f;
          minVal = center - 15.0f;
        }

        for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
          int idx     = (graphIdx + i)     % GRAPH_WIDTH;
          int nextIdx = (graphIdx + i + 1) % GRAPH_WIDTH;
          float n1 = fmaxf(0, fminf(1, (dragHistory[idx]     - minVal) / (maxVal - minVal)));
          float n2 = fmaxf(0, fminf(1, (dragHistory[nextIdx] - minVal) / (maxVal - minVal)));
          int x1 = gX + (int)((float)i       / GRAPH_WIDTH * gW);
          int x2 = gX + (int)((float)(i + 1) / GRAPH_WIDTH * gW);
          int y1 = gY - (int)(n1 * gH);
          int y2 = gY - (int)(n2 * gH);
          // Fade older samples
          unsigned char alpha = (unsigned char)(100 + 120 * ((float)i / GRAPH_WIDTH));
          DrawLine(x1, y1, x2, y2, (Color){255, 80, 60, alpha});
        }
      }

      // ================================================================
      // TOP-RIGHT: View mode legend pill
      // ================================================================
      {
        const char *hint = "[V] Smoke · Pressure · Velocity · Vorticity";
        int hintW = MeasureText(hint, 9);
        DrawText(hint, sw - hintW - 14, 14, 9, (Color){80, 85, 100, 200});
      }
    }
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
