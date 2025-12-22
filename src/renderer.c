#include "renderer.h"
#include "raylib.h"

void Render_Fluid(Fluid *fluid, int scale) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {

      // Draw Car
      if (fluid->obstacles[IX(i, j)]) {
        DrawRectangle(i * scale, j * scale, scale, scale, RED);
        continue;
      }

      // Draw Smoke
      float d = fluid->density[IX(i, j)];
      if (d > 0.1f) {
        if (d > 255.0f)
          d = 255.0f;
        unsigned char val = (unsigned char)d;

        // FIX: Gray Smoke (R=val, G=val, B=val)
        Color color = (Color){val, val, val, 255};
        DrawRectangle(i * scale, j * scale, scale, scale, color);
      }
    }
  }
}
