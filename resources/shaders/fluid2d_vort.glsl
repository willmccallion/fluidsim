#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// texVel is staggered (W+1)x(H+1): u on left faces, v on bottom faces.
// The confinement force is evaluated at cell centres and averaged onto faces.
layout(binding = 0, rgba32f) uniform image2D texVel;
layout(binding = 1, r32f) uniform readonly image2D texCurl;

uniform float dt;
uniform float curlStrength;

float CurlAt(ivec2 c) {
    return imageLoad(texCurl, clamp(c, ivec2(0), imageSize(texCurl) - 1)).r;
}

vec2 ConfinementForce(ivec2 c) {
    float cC = CurlAt(c);

    // Gradient of the magnitude of curl
    float dC_dx = abs(CurlAt(c + ivec2(1, 0))) - abs(CurlAt(c - ivec2(1, 0)));
    float dC_dy = abs(CurlAt(c + ivec2(0, 1))) - abs(CurlAt(c - ivec2(0, 1)));

    vec2 forceDir = vec2(dC_dx, dC_dy);
    float len = length(forceDir);
    if (len <= 0.0001) return vec2(0.0);

    // The force is perpendicular to the gradient: (N x w) in 2D
    forceDir /= len;
    return vec2(forceDir.y, -forceDir.x) * cC * curlStrength;
}

void main() {
    ivec2 f = ivec2(gl_GlobalInvocationID.xy);
    ivec2 cells = imageSize(texCurl);
    if (f.x > cells.x || f.y > cells.y) return;

    vec4 velocity = imageLoad(texVel, f);
    // Interior faces only; boundary faces keep their prescribed values.
    if (f.x > 0 && f.x < cells.x && f.y < cells.y)
        velocity.x += 0.5 * (ConfinementForce(f - ivec2(1, 0)).x + ConfinementForce(f).x) * dt;
    if (f.y > 0 && f.y < cells.y && f.x < cells.x)
        velocity.y += 0.5 * (ConfinementForce(f - ivec2(0, 1)).y + ConfinementForce(f).y) * dt;
    imageStore(texVel, f, velocity);
}
