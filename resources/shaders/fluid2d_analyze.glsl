#version 430

// --- AUTO-RANGE ANALYZER ---
// Scans the fluid to find the highest Pressure and Velocity.
// This allows the display to auto-scale the colors (Normalization).

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r16f) uniform image2D texPressure;
layout(binding = 1, rgba16f) uniform image2D texVelocity;
layout(binding = 2, r16f) uniform image2D texCurl;

// Output Buffer (Atomic Max requires uint)
layout(std430, binding = 3) buffer StatsBuffer {
    uint maxPressureInt;
    uint maxVelocityInt;
    uint maxCurlInt;
};

uniform vec2 res;

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    if (uv.x >= int(res.x) || uv.y >= int(res.y)) return;

    // 1. Sample Data
    float p = abs(imageLoad(texPressure, uv).r);
    float v = length(imageLoad(texVelocity, uv).xy);
    float c = abs(imageLoad(texCurl, uv).r);

    // 2. Atomic Max
    atomicMax(maxPressureInt, floatBitsToUint(p));
    atomicMax(maxVelocityInt, floatBitsToUint(v));
    atomicMax(maxCurlInt, floatBitsToUint(c));
}
