#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba16f) uniform readonly image2D texVel;
layout(binding = 1, r16f) uniform writeonly image2D texCurl;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);

    if (coords.x >= size.x || coords.y >= size.y) return;

    ivec2 L = clamp(coords - ivec2(1,0), ivec2(0), size - 1);
    ivec2 R = clamp(coords + ivec2(1,0), ivec2(0), size - 1);
    ivec2 D = clamp(coords - ivec2(0,1), ivec2(0), size - 1);
    ivec2 U = clamp(coords + ivec2(0,1), ivec2(0), size - 1);

    vec2 vL = imageLoad(texVel, L).xy;
    vec2 vR = imageLoad(texVel, R).xy;
    vec2 vD = imageLoad(texVel, D).xy;
    vec2 vU = imageLoad(texVel, U).xy;

    // Curl in 2D = d(vy)/dx - d(vx)/dy
    float curl = (vR.y - vL.y) - (vU.x - vD.x);
    // Multiply by 0.5 for central difference? Often skipped in games for stronger effect.
    
    imageStore(texCurl, coords, vec4(curl, 0, 0, 0));
}
