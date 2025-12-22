#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r16f) uniform image2D texP;
layout(binding = 1, r16f) uniform image2D texDiv;
layout(binding = 2, r16f) uniform image2D texObs;
layout(binding = 3, r16f) uniform writeonly image2D texDest;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texP);

    // If this cell is an obstacle, pressure is 0 (irrelevant)
    if (imageLoad(texObs, coords).r > 0.5) {
        imageStore(texDest, coords, vec4(0));
        return;
    }

    // Get Center Pressure
    float pC = imageLoad(texP, coords).r;

    // Neighbor Coordinates
    ivec2 L = coords - ivec2(1,0);
    ivec2 R = coords + ivec2(1,0);
    ivec2 D = coords - ivec2(0,1);
    ivec2 U = coords + ivec2(0,1);

    // --- CRITICAL FIX: NEUMANN BOUNDARY CONDITIONS ---
    // If a neighbor is an obstacle (or out of bounds), use pC (Center Pressure).
    // This effectively makes the derivative 0 at the wall, reflecting pressure.
    
    float pL = (L.x < 0 || imageLoad(texObs, L).r > 0.5) ? pC : imageLoad(texP, L).r;
    float pR = (R.x >= size.x || imageLoad(texObs, R).r > 0.5) ? pC : imageLoad(texP, R).r;
    float pD = (D.y < 0 || imageLoad(texObs, D).r > 0.5) ? pC : imageLoad(texP, D).r;
    float pU = (U.y >= size.y || imageLoad(texObs, U).r > 0.5) ? pC : imageLoad(texP, U).r;

    float div = imageLoad(texDiv, coords).r;

    // Standard Jacobi Formula
    float pNew = (pL + pR + pD + pU - div) * 0.25;
    
    imageStore(texDest, coords, vec4(pNew, 0, 0, 0));
}
