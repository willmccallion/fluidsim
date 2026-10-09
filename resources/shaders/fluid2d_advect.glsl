#version 430

// --- MACCORMACK ADVECTION OF CELL-CENTRED FIELDS (smoke) ---
// Performs a forward advection, then a backward advection, measures the
// error, and corrects it, with a limiter against overshoot.

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D texVelocity; // staggered, (W+1)x(H+1)
layout(binding = 2) uniform sampler2D texSource;   // cell-centred, WxH

layout(binding = 3, rgba32f) uniform writeonly image2D texDest;

uniform float dt;

// Centre of the first inlet column, where inflowing values are prescribed.
const float INFLOW_EDGE_X = 0.5;

// Staggered velocity: texel (i, j) holds u on the left face of cell (i, j)
// in .x and v on its bottom face in .y. Points are in cell units.
vec2 SampleVelocity(vec2 p) {
    vec2 size = vec2(textureSize(texVelocity, 0));
    float u = texture(texVelocity, (p + vec2(0.5, 0.0)) / size).x;
    float v = texture(texVelocity, (p + vec2(0.0, 0.5)) / size).y;
    return vec2(u, v);
}

vec4 SampleSource(vec2 p) {
    return texture(texSource, p / vec2(textureSize(texSource, 0)));
}

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(coords, imageSize(texDest)))) return;
    vec2 pos = vec2(coords) + 0.5;

    // 1. Velocity at the cell centre (cells per second)
    vec2 vel = SampleVelocity(pos);

    // 2. Trace back along the flow. A trace that leaves through the inflow
    // edge stops there: that fluid entered partway through the step.
    vec2 displacement = vel * dt;
    float traceFraction = 1.0;
    if (displacement.x > 0.0 && pos.x - displacement.x < INFLOW_EDGE_X)
        traceFraction = max(pos.x - INFLOW_EDGE_X, 0.0) / displacement.x;
    vec2 src = pos - displacement * traceFraction;

    // 3. Forward estimate
    vec4 forward = SampleSource(src);

    // 4. Trace forward again from the source and measure the round-trip error
    vec2 back = src + SampleVelocity(src) * dt * traceFraction;
    vec4 error = (SampleSource(back) - SampleSource(pos)) * 0.5;
    vec4 result = forward - error;

    // 5. Limiter: clamp to the neighbours of the source point
    vec4 s0 = SampleSource(src + vec2(-1.0, 0.0));
    vec4 s1 = SampleSource(src + vec2( 1.0, 0.0));
    vec4 s2 = SampleSource(src + vec2(0.0, -1.0));
    vec4 s3 = SampleSource(src + vec2(0.0,  1.0));
    result = clamp(result, min(min(s0, s1), min(s2, s3)),
                   max(max(s0, s1), max(s2, s3)));

    // 6. Decay (very gentle — keep smoke bright), 0.9995 per 0.005 s of sim time
    result *= pow(0.9995, dt / 0.005);

    imageStore(texDest, coords, result);
}
