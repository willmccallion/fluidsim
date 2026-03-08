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

  Shader shdDisplay = LoadShader(0, "resources/shaders/display.fs");

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

    // Wind Speed (F/G keys)
    if (IsKeyDown(KEY_F))
      sim.windSpeed = fmaxf(50.0f, sim.windSpeed - 30.0f);
    if (IsKeyDown(KEY_G))
      sim.windSpeed = fminf(4000.0f, sim.windSpeed + 30.0f);

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

    // 3. Draw Obstacles (Red Glow)
    Texture2D obsTex = {0};
    obsTex.id = sim.texObstacles.id;
    obsTex.width = RES_X;
    obsTex.height = RES_Y;
    obsTex.format = PIXELFORMAT_UNCOMPRESSED_R32;
    obsTex.mipmaps = 1;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(obsTex, (Rectangle){0, 0, (float)RES_X, (float)-RES_Y},
                   (Rectangle){0, 0, 1280, 640}, (Vector2){0, 0}, 0.0f, RED);
    EndBlendMode();

    // 4. UI & HUD
    if (showHUD) {
      int sw = GetScreenWidth();
      int sh = GetScreenHeight();

      // --- Top-left info panel ---
      int panelX = 12, panelY = 12, panelW = 310, panelH = 110;
      DrawRectangleRounded((Rectangle){panelX, panelY, panelW, panelH}, 0.12f, 8, (Color){0, 0, 0, 160});
      DrawRectangleRoundedLines((Rectangle){panelX, panelY, panelW, panelH}, 0.12f, 8, (Color){255, 255, 255, 30});

      // FPS
      DrawText(TextFormat("%d fps", GetFPS()), panelX + 12, panelY + 10, 14, (Color){160, 255, 160, 255});

      // View mode
      const char *modeName = "RGB Density";
      Color modeColor = RAYWHITE;
      if (viewMode == 1) { modeName = "Pressure Field"; modeColor = (Color){100, 180, 255, 255}; }
      if (viewMode == 2) { modeName = "Velocity Field"; modeColor = (Color){255, 200, 80, 255}; }
      if (viewMode == 3) { modeName = "Vorticity"; modeColor = (Color){255, 100, 180, 255}; }
      DrawText(TextFormat("VIEW  %s", modeName), panelX + 12, panelY + 30, 16, modeColor);

      // Brush size bar
      DrawText("BRUSH", panelX + 12, panelY + 54, 11, (Color){150, 150, 150, 255});
      float bNorm = (brushRadius - 10.0f) / 290.0f;
      DrawRectangle(panelX + 60, panelY + 54, 220, 10, (Color){40, 40, 40, 255});
      DrawRectangle(panelX + 60, panelY + 54, (int)(bNorm * 220), 10, (Color){120, 200, 255, 200});
      DrawText(TextFormat("%.0f", brushRadius), panelX + 285, panelY + 52, 11, (Color){180, 180, 180, 255});

      // Wind speed bar (only meaningful in wind tunnel mode)
      const char *windLabel = sim.enableWindTunnel ? "WIND " : "WIND ";
      Color windBarColor = sim.enableWindTunnel ? (Color){255, 160, 60, 200} : (Color){80, 80, 80, 150};
      DrawText(windLabel, panelX + 12, panelY + 72, 11, sim.enableWindTunnel ? (Color){255, 160, 60, 255} : (Color){100, 100, 100, 255});
      float wNorm = (sim.windSpeed - 50.0f) / 3950.0f;
      DrawRectangle(panelX + 60, panelY + 72, 220, 10, (Color){40, 40, 40, 255});
      DrawRectangle(panelX + 60, panelY + 72, (int)(wNorm * 220), 10, windBarColor);
      DrawText(TextFormat("%.0f", sim.windSpeed), panelX + 285, panelY + 70, 11, (Color){180, 180, 180, 255});

      // Keybind hint strip
      DrawText("V view  Scroll brush  F/G wind  W tunnel  Space ptcl  RMB wall  MMB erase  H hide", panelX + 12, panelY + 90, 9, (Color){120, 120, 120, 200});

      // --- Wind Tunnel Telemetry (bottom-left) ---
      if (sim.enableWindTunnel) {
        int telX = 12, telY = sh - 160, telW = 320, telH = 148;
        DrawRectangleRounded((Rectangle){telX, telY, telW, telH}, 0.10f, 8, (Color){0, 0, 0, 160});
        DrawRectangleRoundedLines((Rectangle){telX, telW, telW, telH}, 0.10f, 8, (Color){255, 255, 255, 25});

        // Drag & Lift values
        DrawText("AERODYNAMICS", telX + 12, telY + 10, 11, (Color){150, 150, 150, 255});
        DrawText(TextFormat("Drag  %.3f", smoothedDrag), telX + 12, telY + 28, 18, (Color){255, 100, 80, 255});
        Color liftColor = forces.y < 0 ? (Color){80, 180, 255, 255} : (Color){255, 200, 60, 255};
        DrawText(TextFormat("Lift  %.3f", forces.y), telX + 12, telY + 50, 18, liftColor);

        // Graph
        int graphX = telX + 12, graphY = telY + 140, graphH = 70, graphW = 296;
        DrawRectangle(graphX, graphY - graphH, graphW, graphH, (Color){10, 10, 20, 200});

        float minVal = dragHistory[0], maxVal = dragHistory[0];
        for (int i = 0; i < GRAPH_WIDTH; i++) {
          if (dragHistory[i] < minVal) minVal = dragHistory[i];
          if (dragHistory[i] > maxVal) maxVal = dragHistory[i];
        }
        if (maxVal - minVal < 50.0f) {
          float center = (maxVal + minVal) * 0.5f;
          maxVal = center + 25.0f;
          minVal = center - 25.0f;
        }

        for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
          int idx = (graphIdx + i) % GRAPH_WIDTH;
          int nextIdx = (graphIdx + i + 1) % GRAPH_WIDTH;
          float norm1 = (dragHistory[idx] - minVal) / (maxVal - minVal);
          float norm2 = (dragHistory[nextIdx] - minVal) / (maxVal - minVal);
          int x1 = graphX + (int)((float)i / GRAPH_WIDTH * graphW);
          int x2 = graphX + (int)((float)(i + 1) / GRAPH_WIDTH * graphW);
          int y1 = graphY - (int)(fmaxf(0, fminf(1, norm1)) * graphH);
          int y2 = graphY - (int)(fmaxf(0, fminf(1, norm2)) * graphH);
          DrawLine(x1, y1, x2, y2, (Color){255, 100, 80, 220});
        }
        DrawText("drag history", graphX + 4, graphY - graphH + 4, 9, (Color){100, 100, 100, 200});
      }
    }
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
