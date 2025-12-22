#include "fluid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// --- Helper: Draw Lines ---
static void DrawLine(bool *grid, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  while (1) {
    if (x0 >= 0 && x0 < N && y0 >= 0 && y0 < N)
      grid[IX(x0, y0)] = true;
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

Fluid *Fluid_Create(float dt, float diffusion, float viscosity) {
  Fluid *fluid = (Fluid *)malloc(sizeof(Fluid));
  fluid->size = N;
  fluid->dt = dt;
  fluid->diff = diffusion;
  fluid->visc = viscosity;

  fluid->s = (float *)calloc(N * N, sizeof(float));
  fluid->density = (float *)calloc(N * N, sizeof(float));
  fluid->Vx = (float *)calloc(N * N, sizeof(float));
  fluid->Vy = (float *)calloc(N * N, sizeof(float));
  fluid->Vx0 = (float *)calloc(N * N, sizeof(float));
  fluid->Vy0 = (float *)calloc(N * N, sizeof(float));
  fluid->obstacles = (bool *)calloc(N * N, sizeof(bool));

  // --- TUNED: Velocity -5.0f ---
  // Reduced from -20.0f to -5.0f.
  // -20.0f was moving ~25 pixels per frame, causing gaps/strobing.
  // -5.0f provides a smooth, continuous stream.
  for (int i = 0; i < N * N; i++) {
    fluid->Vx[i] = -5.0f;
    fluid->Vx0[i] = -5.0f;
  }

  return fluid;
}

void Fluid_Free(Fluid *fluid) {
  free(fluid->s);
  free(fluid->density);
  free(fluid->Vx);
  free(fluid->Vy);
  free(fluid->Vx0);
  free(fluid->Vy0);
  free(fluid->obstacles);
  free(fluid);
}

void Fluid_AddCarObstacle(Fluid *fluid) {
  memset(fluid->obstacles, 0, N * N * sizeof(bool));
  int cx = N / 2 + 20;
  int cy = N / 2;

  // F1 Shape
  DrawLine(fluid->obstacles, cx + 30, cy - 10, cx + 30, cy + 10);
  DrawLine(fluid->obstacles, cx + 30, cy, cx - 10, cy);
  DrawLine(fluid->obstacles, cx + 10, cy - 6, cx - 10, cy - 6);
  DrawLine(fluid->obstacles, cx + 10, cy + 6, cx - 10, cy + 6);
  DrawLine(fluid->obstacles, cx - 25, cy - 10, cx - 25, cy + 10);

  // Tires
  for (int x = cx + 10; x < cx + 20; x++) {
    for (int y = cy - 12; y < cy - 8; y++)
      fluid->obstacles[IX(x, y)] = true;
    for (int y = cy + 8; y < cy + 12; y++)
      fluid->obstacles[IX(x, y)] = true;
  }
  for (int x = cx - 25; x < cx - 15; x++) {
    for (int y = cy - 12; y < cy - 8; y++)
      fluid->obstacles[IX(x, y)] = true;
    for (int y = cy + 8; y < cy + 12; y++)
      fluid->obstacles[IX(x, y)] = true;
  }
}

static void set_bnd(int b, float *x, bool *obstacles) {
  for (int i = 1; i < N - 1; i++) {
    x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
    x[IX(i, N - 1)] = b == 2 ? -x[IX(i, N - 2)] : x[IX(i, N - 2)];
  }

  for (int j = 1; j < N - 1; j++) {
    x[IX(0, j)] = x[IX(1, j)];

    // RIGHT WALL (Inflow)
    if (b == 1) {
      // Must match the initialization velocity (-5.0f)
      x[IX(N - 1, j)] = -5.0f;
    } else {
      x[IX(N - 1, j)] = x[IX(N - 2, j)];
    }
  }

  x[IX(0, 0)] = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
  x[IX(0, N - 1)] = 0.5f * (x[IX(1, N - 1)] + x[IX(0, N - 2)]);
  x[IX(N - 1, 0)] = 0.5f * (x[IX(N - 2, 0)] + x[IX(N - 1, 1)]);
  x[IX(N - 1, N - 1)] = 0.5f * (x[IX(N - 2, N - 1)] + x[IX(N - 1, N - 2)]);

  for (int i = 1; i < N - 1; i++) {
    for (int j = 1; j < N - 1; j++) {
      if (obstacles[IX(i, j)]) {
        x[IX(i, j)] = 0.0f;
      }
    }
  }
}

static void lin_solve(int b, float *x, float *x0, float a, float c, bool *obs) {
  float cRecip = 1.0f / c;
  for (int k = 0; k < ITER; k++) {
    for (int j = 1; j < N - 1; j++) {
      for (int i = 1; i < N - 1; i++) {
        if (obs[IX(i, j)]) {
          x[IX(i, j)] = 0;
          continue;
        }
        x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i + 1, j)] + x[IX(i - 1, j)] +
                                           x[IX(i, j + 1)] + x[IX(i, j - 1)])) *
                      cRecip;
      }
    }
    set_bnd(b, x, obs);
  }
}

static void project(float *velocX, float *velocY, float *p, float *div,
                    bool *obs) {
  for (int j = 1; j < N - 1; j++) {
    for (int i = 1; i < N - 1; i++) {
      if (obs[IX(i, j)]) {
        div[IX(i, j)] = 0;
        p[IX(i, j)] = 0;
        continue;
      }
      div[IX(i, j)] = -0.5f *
                      (velocX[IX(i + 1, j)] - velocX[IX(i - 1, j)] +
                       velocY[IX(i, j + 1)] - velocY[IX(i, j - 1)]) /
                      N;
      p[IX(i, j)] = 0;
    }
  }
  set_bnd(0, div, obs);
  set_bnd(0, p, obs);
  lin_solve(0, p, div, 1, 4, obs);
  for (int j = 1; j < N - 1; j++) {
    for (int i = 1; i < N - 1; i++) {
      if (obs[IX(i, j)]) {
        velocX[IX(i, j)] = 0;
        velocY[IX(i, j)] = 0;
        continue;
      }
      velocX[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * N;
      velocY[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * N;
    }
  }
  set_bnd(1, velocX, obs);
  set_bnd(2, velocY, obs);
}

static void advect(int b, float *d, float *d0, float *velocX, float *velocY,
                   float dt, bool *obs) {
  float i0, i1, j0, j1, dtx = dt * (N - 2), dty = dt * (N - 2);
  float s0, s1, t0, t1, tmp1, tmp2, x, y, Nfloat = N, ifloat, jfloat;
  int i, j;
  for (j = 1, jfloat = 1; j < N - 1; j++, jfloat++) {
    for (i = 1, ifloat = 1; i < N - 1; i++, ifloat++) {
      if (obs[IX(i, j)]) {
        d[IX(i, j)] = 0;
        continue;
      }
      tmp1 = dtx * velocX[IX(i, j)];
      tmp2 = dty * velocY[IX(i, j)];
      x = ifloat - tmp1;
      y = jfloat - tmp2;
      if (x < 0.5f)
        x = 0.5f;
      if (x > Nfloat + 0.5f)
        x = Nfloat + 0.5f;
      i0 = floorf(x);
      i1 = i0 + 1.0f;
      if (y < 0.5f)
        y = 0.5f;
      if (y > Nfloat + 0.5f)
        y = Nfloat + 0.5f;
      j0 = floorf(y);
      j1 = j0 + 1.0f;
      s1 = x - i0;
      s0 = 1.0f - s1;
      t1 = y - j0;
      t0 = 1.0f - t1;
      d[IX(i, j)] =
          s0 * (t0 * d0[IX((int)i0, (int)j0)] + t1 * d0[IX((int)i0, (int)j1)]) +
          s1 * (t0 * d0[IX((int)i1, (int)j0)] + t1 * d0[IX((int)i1, (int)j1)]);
    }
  }
  set_bnd(b, d, obs);
}

void Fluid_InjectStreamlines(Fluid *fluid, int interval) {
  for (int j = 1; j < N - 1; j += interval) {
    // Inject at Right Edge
    fluid->density[IX(N - 2, j)] = 255.0f;
    fluid->density[IX(N - 3, j)] = 255.0f;
  }
}

void Fluid_Step(Fluid *fluid) {
  float dt = fluid->dt;
  float visc = fluid->visc;
  float diff = fluid->diff; // Use diffusion
  bool *obs = fluid->obstacles;

  // 1. Copy Velocity
  memcpy(fluid->Vx0, fluid->Vx, N * N * sizeof(float));
  memcpy(fluid->Vy0, fluid->Vy, N * N * sizeof(float));

  // 2. Diffuse Velocity
  // FIX: Calculate 'a' and 'c' properly.
  // If visc is 0, a=0, c=1, which means no change (correct).
  float a = dt * visc * (N - 2) * (N - 2);
  lin_solve(1, fluid->Vx, fluid->Vx0, a, 1 + 4 * a, obs);
  lin_solve(2, fluid->Vy, fluid->Vy0, a, 1 + 4 * a, obs);

  // 3. Project
  project(fluid->Vx, fluid->Vy, fluid->Vx0, fluid->Vy0, obs);

  // 4. Copy for Advection
  memcpy(fluid->Vx0, fluid->Vx, N * N * sizeof(float));
  memcpy(fluid->Vy0, fluid->Vy, N * N * sizeof(float));

  // 5. Advect Velocity
  advect(1, fluid->Vx, fluid->Vx0, fluid->Vx0, fluid->Vy0, dt, obs);
  advect(2, fluid->Vy, fluid->Vy0, fluid->Vx0, fluid->Vy0, dt, obs);

  // 6. Project
  project(fluid->Vx, fluid->Vy, fluid->Vx0, fluid->Vy0, obs);

  // 7. Copy Density
  memcpy(fluid->s, fluid->density, N * N * sizeof(float));

  // 8. Diffuse Density (Optional, usually 0 for smoke)
  float a_d = dt * diff * (N - 2) * (N - 2);
  lin_solve(0, fluid->density, fluid->s, a_d, 1 + 4 * a_d, obs);

  // 9. Advect Density
  // Note: We advect 'density' using 's' as the source
  advect(0, fluid->density, fluid->s, fluid->Vx, fluid->Vy, dt, obs);
}
