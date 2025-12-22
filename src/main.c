#include "fluid.h"
#include "raylib.h"
#include "renderer.h"

#define SCALE 6

int main(void) {
  const int screenWidth = N * SCALE;
  const int screenHeight = N * SCALE;

  InitWindow(screenWidth, screenHeight, "F1 Wind Tunnel (Stable)");
  SetTargetFPS(60);

  // --- CRITICAL FIX: Lower dt to 0.01f ---
  // This prevents the "Explosion/Flashing" bug.
  // dt=0.01, diff=0, visc=0
  Fluid *fluid = Fluid_Create(0.01f, 0.0f, 0.0f);

  Fluid_AddCarObstacle(fluid);

  while (!WindowShouldClose()) {
    // 1. Inject Smoke (From Right)
    Fluid_InjectStreamlines(fluid, 8);

    // 2. Physics Step
    Fluid_Step(fluid);

    // 3. Decay (Slower decay since dt is smaller)
    // 0.995 keeps the smoke trail visible longer
    for (int k = 0; k < N * N; k++)
      fluid->density[k] *= 0.995f;

    // 4. Render
    BeginDrawing();
    ClearBackground(BLACK);
    Render_Fluid(fluid, SCALE);

    DrawFPS(10, 10);
    DrawText("Wind: Right -> Left", 10, 30, 20, DARKGRAY);
    EndDrawing();
  }

  Fluid_Free(fluid);
  CloseWindow();
  return 0;
}
