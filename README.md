# GPU Fluid Simulation

Real-time 2D incompressible fluid solver using OpenGL 4.3 compute shaders. Cell-centred Navier-Stokes with MacCormack advection, multigrid pressure projection, and vorticity confinement at 2560×1280 resolution.

## Features

- GPU-accelerated solver (all passes are compute shaders)
- MacCormack advection for sharp detail
- Interactive force/dye painting and obstacle placement
- Wind tunnel mode with drag/lift force analysis
- Up to 50k GPU-driven tracer particles
- Visualization modes: RGB density, pressure field, velocity magnitude
- Headless PNG export of smoke lines past a circle (`--png`)

## Building

Requires CMake 3.14+ and OpenGL 4.3. Raylib 5.5 is fetched automatically.
On Nix, `nix develop` provides the toolchain and X11/GL/EGL libraries.

```bash
cmake -B build
cmake --build build
./build/fluid_sim
```

## Rendering images

`--png` simulates air flowing past a circle without opening a window (via
EGL) and writes the smoke lines, coloured by speed, as
`output/sphere-<palette>.png`. Run it from the repository root so it can find
`resources/`.

```bash
./build/fluid_sim --png                            # every palette, 1280x1280
./build/fluid_sim --png --scale 2 --palette psych  # one palette, 2560x2560
./build/fluid_sim --png --help                     # all options and palettes
```

`--scale` multiplies the grid resolution, which gives a more turbulent wake
and a larger image. Cost grows roughly with the cube of the scale: about 40 s
at scale 1 and 5.5 min at scale 2 on an RTX 4060 Ti.

## Controls

| Key | Action |
|-----|--------|
| Left drag | Add dye and velocity |
| Right drag | Paint obstacles |
| `V` | Cycle visualization mode |
| `W` | Toggle wind tunnel |
| `Space` | Toggle particles |
| `1` / `2` | Reset free mode / wind tunnel |
