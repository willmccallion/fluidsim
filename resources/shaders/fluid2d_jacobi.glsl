#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r16f) uniform image2D texP;
layout(binding = 1, r16f) uniform image2D texDiv;
layout(binding = 2, r16f) uniform image2D texObs;
layout(binding = 3, r16f) uniform writeonly image2D texDest;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texP);

    // Obstacles have 0 pressure
    if (imageLoad(texObs, coords).r > 0.5) {
        imageStore(texDest, coords, vec4(0));
        return;
    }

    float pC = imageLoad(texP, coords).r;

    // Neighbor Coords
    ivec2 L = coords - ivec2(1,0);
    ivec2 R = coords + ivec2(1,0);
    ivec2 D = coords - ivec2(0,1);
    ivec2 U = coords + ivec2(0,1);

    // --- BOUNDARY CONDITIONS ---
    
    // Left (Inlet), Top, Bottom: Closed/Neumann (Copy Center Pressure)
    float pL = (L.x < 0 || imageLoad(texObs, L).r > 0.5) ? pC : imageLoad(texP, L).r;
    float pD = (D.y < 0 || imageLoad(texObs, D).r > 0.5) ? pC : imageLoad(texP, D).r;
    float pU = (U.y >= size.y || imageLoad(texObs, U).r > 0.5) ? pC : imageLoad(texP, U).r;

    // --- FIX: OPEN OUTLET (Dirichlet) ---
    // If the Right neighbor is out of bounds, assume Pressure is 0.0 (Atmosphere).
    // This pins the system and stops the flashing.
    float pR;
    if (R.x >= size.x) {
        pR = 0.0; 
    } else if (imageLoad(texObs, R).r > 0.5) {
        pR = pC;
    } else {
        pR = imageLoad(texP, R).r;
    }

    float div = imageLoad(texDiv, coords).r;

    // Jacobi Formula
    float pNew = (pL + pR + pD + pU - div) * 0.25;
    
    imageStore(texDest, coords, vec4(pNew, 0, 0, 0));
}
