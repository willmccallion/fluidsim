#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// texVel is staggered (W+1)x(H+1): u on left faces, v on bottom faces.
layout(binding = 0, rgba32f) uniform readonly image2D texVel;
layout(binding = 1, r32f) uniform writeonly image2D texCurl;

vec2 CellVelocity(ivec2 c) {
    float u = 0.5 * (imageLoad(texVel, c).x + imageLoad(texVel, c + ivec2(1, 0)).x);
    float v = 0.5 * (imageLoad(texVel, c).y + imageLoad(texVel, c + ivec2(0, 1)).y);
    return vec2(u, v);
}

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texCurl);

    if (coords.x >= size.x || coords.y >= size.y) return;

    ivec2 L = clamp(coords - ivec2(1,0), ivec2(0), size - 1);
    ivec2 R = clamp(coords + ivec2(1,0), ivec2(0), size - 1);
    ivec2 D = clamp(coords - ivec2(0,1), ivec2(0), size - 1);
    ivec2 U = clamp(coords + ivec2(0,1), ivec2(0), size - 1);

    vec2 vL = CellVelocity(L);
    vec2 vR = CellVelocity(R);
    vec2 vD = CellVelocity(D);
    vec2 vU = CellVelocity(U);

    // Curl in 2D = d(vy)/dx - d(vx)/dy
    float curl = (vR.y - vL.y) - (vU.x - vD.x);
    // Multiply by 0.5 for central difference? Often skipped in games for stronger effect.
    
    imageStore(texCurl, coords, vec4(curl, 0, 0, 0));
}
