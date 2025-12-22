#version 430
layout (local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, rgba16f) uniform image2D texVel;
layout(binding = 1, r16f)    uniform image2D texObs;
layout(binding = 2, r16f)    uniform writeonly image2D texDiv;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);
    
    // Clamp coordinates to stay inside the image
    // This implements "Open Boundary" conditions implicitly
    ivec2 L = clamp(coords - ivec2(1,0), ivec2(0), size - 1);
    ivec2 R = clamp(coords + ivec2(1,0), ivec2(0), size - 1);
    ivec2 D = clamp(coords - ivec2(0,1), ivec2(0), size - 1);
    ivec2 U = clamp(coords + ivec2(0,1), ivec2(0), size - 1);
    
    vec2 vL = imageLoad(texVel, L).xy;
    vec2 vR = imageLoad(texVel, R).xy;
    vec2 vD = imageLoad(texVel, D).xy;
    vec2 vU = imageLoad(texVel, U).xy;
    
    // Handle Internal Obstacles
    if (imageLoad(texObs, L).r > 0.5) vL = vec2(0);
    if (imageLoad(texObs, R).r > 0.5) vR = vec2(0);
    if (imageLoad(texObs, D).r > 0.5) vD = vec2(0);
    if (imageLoad(texObs, U).r > 0.5) vU = vec2(0);

    float div = 0.5 * ((vR.x - vL.x) + (vU.y - vD.y));
    imageStore(texDiv, coords, vec4(div, 0, 0, 0));
}
