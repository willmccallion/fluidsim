#version 430

// --- AUTO-RANGE ANALYZER ---
// Scans the fluid to find the highest Pressure and Velocity.
// This allows the display to auto-scale the colors (Normalization).

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r16f) uniform image2D texPressure;
layout(binding = 1, rgba16f) uniform image2D texVelocity;

// Output Buffer (Atomic Max requires uint)
layout(std430, binding = 3) buffer StatsBuffer {
    uint maxPressureInt;
    uint maxVelocityInt;
};

uniform vec2 res;

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    if (uv.x >= int(res.x) || uv.y >= int(res.y)) return;

    // 1. Sample Data
    float p = abs(imageLoad(texPressure, uv).r);
    float v = length(imageLoad(texVelocity, uv).xy);

    // 2. Atomic Max
    // We convert float bits to uint to perform the atomic comparison.
    // Since we used abs() and length(), values are positive, so this sort works correctly.
    atomicMax(maxPressureInt, floatBitsToUint(p));
    atomicMax(maxVelocityInt, floatBitsToUint(v));
}
