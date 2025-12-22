#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r16f) uniform image2D texP;
layout(binding = 1, rgba16f) uniform image2D texVel;
layout(binding = 2, r16f) uniform image2D texObs;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texP);

    if (imageLoad(texObs, coords).r > 0.5) return;

    float pC = imageLoad(texP, coords).r;

    ivec2 L = coords - ivec2(1,0);
    ivec2 R = coords + ivec2(1,0);
    ivec2 D = coords - ivec2(0,1);
    ivec2 U = coords + ivec2(0,1);

    // Use Neumann boundaries again to match the solver
    float pL = (L.x < 0 || imageLoad(texObs, L).r > 0.5) ? pC : imageLoad(texP, L).r;
    float pR = (R.x >= size.x || imageLoad(texObs, R).r > 0.5) ? pC : imageLoad(texP, R).r;
    float pD = (D.y < 0 || imageLoad(texObs, D).r > 0.5) ? pC : imageLoad(texP, D).r;
    float pU = (U.y >= size.y || imageLoad(texObs, U).r > 0.5) ? pC : imageLoad(texP, U).r;

    vec2 oldV = imageLoad(texVel, coords).xy;
    
    // Calculate Gradient
    vec2 grad = vec2(pR - pL, pU - pD) * 0.5;
    
    vec2 newV = oldV - grad;

    imageStore(texVel, coords, vec4(newV, 0, 0));
}
