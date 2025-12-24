#version 430

// --- REAL-TIME CFD: FORCE INTEGRATION ---
// Calculates Drag and Lift by summing pressure vectors along obstacle boundaries.

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r16f) uniform image2D texPressure;
layout(binding = 1, r16f) uniform image2D texObstacles;

// We use a buffer to sum up the forces from all threads.
layout(std430, binding = 2) buffer ForceBuffer {
    int dragAtomic;
    int liftAtomic;
};

uniform vec2 res;

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    if (uv.x >= int(res.x) || uv.y >= int(res.y)) return;

    // 1. Calculate the Normal of the obstacle surface
    float o_right = imageLoad(texObstacles, uv + ivec2(1, 0)).r;
    float o_left  = imageLoad(texObstacles, uv - ivec2(1, 0)).r;
    float o_up    = imageLoad(texObstacles, uv + ivec2(0, 1)).r;
    float o_down  = imageLoad(texObstacles, uv - ivec2(0, 1)).r;

    vec2 normal = vec2(o_left - o_right, o_down - o_up);

    // If length is > 0, we are on an edge
    if (length(normal) > 0.0) {
        normal = normalize(normal);

        // 2. Sample Pressure
        float p = imageLoad(texPressure, uv).r;

        // 3. Calculate Force Vector
        // FIX: Pressure acts OPPOSITE to the surface normal (-normal).
        // FIX: Reduced scalar to 100.0 to prevent integer overflow.
        vec2 force = -normal * p * 100.0;

        // 4. Atomic Accumulation
        atomicAdd(dragAtomic, int(force.x));
        atomicAdd(liftAtomic, int(force.y));
    }
}
