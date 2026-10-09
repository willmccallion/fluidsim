#version 430

// --- MACCORMACK ADVECTION OF THE STAGGERED VELOCITY ---
// Texel (i, j) holds u on the left face of cell (i, j) in .x (at x = i,
// y = j + 0.5) and v on its bottom face in .y (at x = i + 0.5, y = j).
// Each component is advected from its own face position.

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D texVelocity; // (W+1)x(H+1)
layout(binding = 1) uniform sampler2D texObstacles; // WxH, > 0.5 = solid

layout(binding = 3, rgba32f) uniform writeonly image2D texDest;

uniform float dt;

const float INFLOW_EDGE_X = 0.5;

float SampleU(vec2 p) {
    vec2 size = vec2(textureSize(texVelocity, 0));
    return texture(texVelocity, (p + vec2(0.5, 0.0)) / size).x;
}

float SampleV(vec2 p) {
    vec2 size = vec2(textureSize(texVelocity, 0));
    return texture(texVelocity, (p + vec2(0.0, 0.5)) / size).y;
}

vec2 SampleVelocity(vec2 p) { return vec2(SampleU(p), SampleV(p)); }

bool IsSolidCell(ivec2 c) {
    ivec2 cells = textureSize(texObstacles, 0);
    if (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, cells))) return false;
    return texelFetch(texObstacles, c, 0).r > 0.5;
}

// Faces touching a solid cell, and the top and bottom walls, carry no flow.
bool IsClosedUFace(ivec2 f) {
    return IsSolidCell(f - ivec2(1, 0)) || IsSolidCell(f);
}

bool IsClosedVFace(ivec2 f) {
    int height = textureSize(texObstacles, 0).y;
    return f.y == 0 || f.y == height || IsSolidCell(f - ivec2(0, 1)) || IsSolidCell(f);
}

// Back-trace clipped at the inflow edge (see fluid2d_advect.glsl).
float TraceFraction(vec2 pos, vec2 displacement) {
    if (displacement.x > 0.0 && pos.x - displacement.x < INFLOW_EDGE_X)
        return max(pos.x - INFLOW_EDGE_X, 0.0) / displacement.x;
    return 1.0;
}

float AdvectU(vec2 pos) {
    vec2 displacement = SampleVelocity(pos) * dt;
    float fraction = TraceFraction(pos, displacement);
    vec2 src = pos - displacement * fraction;
    vec2 back = src + SampleVelocity(src) * dt * fraction;
    float result = SampleU(src) - (SampleU(back) - SampleU(pos)) * 0.5;
    float s0 = SampleU(src + vec2(-1.0, 0.0));
    float s1 = SampleU(src + vec2( 1.0, 0.0));
    float s2 = SampleU(src + vec2(0.0, -1.0));
    float s3 = SampleU(src + vec2(0.0,  1.0));
    return clamp(result, min(min(s0, s1), min(s2, s3)), max(max(s0, s1), max(s2, s3)));
}

float AdvectV(vec2 pos) {
    vec2 displacement = SampleVelocity(pos) * dt;
    float fraction = TraceFraction(pos, displacement);
    vec2 src = pos - displacement * fraction;
    vec2 back = src + SampleVelocity(src) * dt * fraction;
    float result = SampleV(src) - (SampleV(back) - SampleV(pos)) * 0.5;
    float s0 = SampleV(src + vec2(-1.0, 0.0));
    float s1 = SampleV(src + vec2( 1.0, 0.0));
    float s2 = SampleV(src + vec2(0.0, -1.0));
    float s3 = SampleV(src + vec2(0.0,  1.0));
    return clamp(result, min(min(s0, s1), min(s2, s3)), max(max(s0, s1), max(s2, s3)));
}

void main() {
    ivec2 f = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texDest);
    if (any(greaterThanEqual(f, size))) return;
    ivec2 cells = size - 1;
    float decay = pow(0.9995, dt / 0.005);

    float u = 0.0;
    if (f.y < cells.y && !IsClosedUFace(f))
        u = AdvectU(vec2(f) + vec2(0.0, 0.5)) * decay;

    float v = 0.0;
    if (f.x < cells.x && !IsClosedVFace(f))
        v = AdvectV(vec2(f) + vec2(0.5, 0.0)) * decay;

    imageStore(texDest, f, vec4(u, v, 0.0, 0.0));
}
