# GPU Fluid Simulation

Real-time 2D incompressible fluid solver using OpenGL 4.3 compute shaders. Staggered-grid Navier-Stokes with MacCormack advection, Jacobi pressure projection, and vorticity confinement at 2560×1280 resolution.

## Features

- GPU-accelerated solver (all passes are compute shaders)
- MacCormack advection for sharp detail
- Interactive force/dye painting and obstacle placement
- Wind tunnel mode with drag/lift force analysis
- Up to 50k GPU-driven tracer particles
- Visualization modes: RGB density, pressure field, velocity magnitude

## Building

Requires CMake 3.14+ and OpenGL 4.3. Raylib is fetched automatically.

```bash
cmake -B build
cmake --build build
./build/fluid_sim
```

## Controls

| Key | Action |
|-----|--------|
| Left drag | Add dye and velocity |
| Right drag | Paint obstacles |
| `V` | Cycle visualization mode |
| `W` | Toggle wind tunnel |
| `Space` | Toggle particles |
| `1` / `2` | Reset free mode / wind tunnel |
