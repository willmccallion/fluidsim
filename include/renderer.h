#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"
#include "simulation.h"

typedef struct {
  Shader shdRender;
} FluidRenderer;

FluidRenderer InitFluidRenderer(void);
void DrawFluidSim(FluidRenderer *renderer, FluidSim *sim, Camera3D camera);

#endif
