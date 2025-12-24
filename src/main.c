#include "particles.h"
#include "raylib.h"
#include "rlgl.h" // Needed for rlActiveTextureSlot
#include "simulation.h"
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
  // 0 = Standard (RGB), 1 = Pressure Tint, 2 = Velocity Tint
  int viewMode = 0;
  bool showParticles = true;
  float time = 0.0f;

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
      viewMode = (viewMode + 1) % 3;

    // Reset Modes
    if (IsKeyPressed(KEY_ONE)) {
      ResetSim(&sim, 0);
      sim.enableWindTunnel = false;
    }
    if (IsKeyPressed(KEY_TWO)) {
      ResetSim(&sim, 1);
      sim.enableWindTunnel = true;
    }

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
      ApplySplat(&sim, sim.texVelocity[sim.ping], simPos, 65.0f, velAdd);

      // Rainbow Dye
      Color c = ColorFromHSV((float)GetTime() * 100.0f, 0.7f, 0.9f);
      Vector4 colAdd = {(c.r / 255.0f) * 4.0f, (c.g / 255.0f) * 4.0f,
                        (c.b / 255.0f) * 4.0f, 1.0f};
      ApplySplat(&sim, sim.texDensity[sim.ping], simPos, 65.0f, colAdd);
    }

    // Right Click: Draw Wall
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      PaintObstacle(&sim, simPos, 20.0f, false);
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

    // Bind Secondary Texture (Data) to Slot 1
    rlActiveTextureSlot(1);
    if (viewMode == 1)
      glBindTexture(GL_TEXTURE_2D, sim.texPressure[0].id);
    else if (viewMode == 2)
      glBindTexture(GL_TEXTURE_2D, sim.texVelocity[sim.ping].id);
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
    DrawFPS(10, 10);

    const char *modeName = "Standard (RGB)";
    if (viewMode == 1)
      modeName = "Pressure Tint (Auto-Range)";
    if (viewMode == 2)
      modeName = "Velocity Tint (Auto-Range)";

    DrawText(TextFormat("View: %s (Press V)", modeName), 10, 30, 20, RAYWHITE);
    DrawText("Space: Particles | W: Wind | 1/2: Reset", 10, 55, 10, GRAY);

    // 5. Graphing (Only in Wind Tunnel Mode)
    if (sim.enableWindTunnel) {
      char buff[64];
      sprintf(buff, "Drag: %.2f", smoothedDrag);
      DrawText(buff, 10, 80, 20, GREEN);
      sprintf(buff, "Lift: %.2f", forces.y);
      DrawText(buff, 10, 105, 20, (forces.y < 0) ? SKYBLUE : ORANGE);

      // Graph Layout
      int graphX = 10, graphY = 600, graphH = 120, graphW = GRAPH_WIDTH;

      // Background
      DrawRectangle(graphX, graphY - graphH, graphW, graphH,
                    (Color){0, 10, 0, 220});
      DrawRectangleLines(graphX, graphY - graphH, graphW, graphH, DARKGREEN);

      // Find Min/Max for Auto-Scaling
      float minVal = dragHistory[0], maxVal = dragHistory[0];
      for (int i = 0; i < GRAPH_WIDTH; i++) {
        if (dragHistory[i] < minVal)
          minVal = dragHistory[i];
        if (dragHistory[i] > maxVal)
          maxVal = dragHistory[i];
      }

      // Enforce minimum range to prevent zoom-on-noise
      if (maxVal - minVal < 50.0f) {
        float center = (maxVal + minVal) * 0.5f;
        maxVal = center + 25.0f;
        minVal = center - 25.0f;
      }

      // Draw Graph Lines
      for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
        int idx = (graphIdx + i) % GRAPH_WIDTH;
        int nextIdx = (graphIdx + i + 1) % GRAPH_WIDTH;

        float norm1 = (dragHistory[idx] - minVal) / (maxVal - minVal);
        float norm2 = (dragHistory[nextIdx] - minVal) / (maxVal - minVal);

        int y1 = graphY - (int)(norm1 * graphH);
        int y2 = graphY - (int)(norm2 * graphH);

        // Clamp to box
        if (y1 < graphY - graphH)
          y1 = graphY - graphH;
        if (y1 > graphY)
          y1 = graphY;
        if (y2 < graphY - graphH)
          y2 = graphY - graphH;
        if (y2 > graphY)
          y2 = graphY;

        DrawLine(graphX + i, y1, graphX + i + 1, y2, GREEN);
      }
      DrawText("Drag History", graphX + 5, graphY - graphH + 5, 10, GREEN);
    }
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
