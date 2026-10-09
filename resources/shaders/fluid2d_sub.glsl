#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// texVel is staggered (W+1)x(H+1): u on left faces, v on bottom faces.
// Subtracts the face pressure gradient with the boundaries of mg_smooth.glsl:
// inflow, walls and solids are fixed faces; the outlet has ghost p = -p.
layout(binding = 0, r32f) uniform image2D texP;
layout(binding = 1, rgba32f) uniform image2D texVel;
layout(binding = 2, r32f) uniform image2D texObs;

bool IsSolidCell(ivec2 c) {
    ivec2 cells = imageSize(texObs);
    if (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, cells))) return false;
    return imageLoad(texObs, c).r > 0.5;
}

float P(ivec2 c) { return imageLoad(texP, c).r; }

float ProjectU(ivec2 f, ivec2 cells, float u) {
    if (f.y >= cells.y || IsSolidCell(f - ivec2(1, 0)) || IsSolidCell(f)) return 0.0;
    if (f.x == 0) return u;
    if (f.x == cells.x) return u + 2.0 * P(f - ivec2(1, 0));
    return u - (P(f) - P(f - ivec2(1, 0)));
}

float ProjectV(ivec2 f, ivec2 cells, float v) {
    if (f.x >= cells.x || f.y == 0 || f.y >= cells.y) return 0.0;
    if (IsSolidCell(f - ivec2(0, 1)) || IsSolidCell(f)) return 0.0;
    return v - (P(f) - P(f - ivec2(0, 1)));
}

void main() {
    ivec2 f = ivec2(gl_GlobalInvocationID.xy);
    ivec2 cells = imageSize(texP);
    if (f.x > cells.x || f.y > cells.y) return;

    vec2 vel = imageLoad(texVel, f).xy;
    imageStore(texVel, f, vec4(ProjectU(f, cells, vel.x), ProjectV(f, cells, vel.y), 0, 0));
}
