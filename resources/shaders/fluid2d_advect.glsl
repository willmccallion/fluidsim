#version 430

// --- MACCORMACK ADVECTION (The "Pro" Standard) ---
// This technique performs a forward advection, then a backward advection,
// measures the error, and corrects it. This creates extremely sharp swirls.
// Crucially, it includes a LIMITER to prevent the "noise" (instability).

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D texVelocity;
layout(binding = 1) uniform sampler2D texObstacles;
layout(binding = 2) uniform sampler2D texSource; // Density or Velocity

layout(binding = 3, rgba16f) uniform writeonly image2D texDest;

uniform float dt;
uniform float buoyancy;
uniform vec2 res;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    vec2 pos = vec2(coords) + 0.5;
    vec2 uv = pos / res;
    vec2 texelSize = 1.0 / res;

    // 1. Get Velocity
    vec2 vel = texture(texVelocity, uv).xy;

    // 2. Advect Forward (Lagrangian)
    // Where did this particle come from?
    vec2 prevPos = uv - vel * dt * texelSize; // Note: vel is in pixels/sec usually, check scaling
    // If vel is normalized (0..1), remove texelSize. Assuming vel is in pixels:
    // Actually, in your sim, velocity is likely in pixels. 
    // Let's assume velocity is in UV space for safety or convert:
    // Standard approach: uv - (vel * dt * inverse_resolution)
    vec2 srcUV = uv - (vel * dt * texelSize);

    // 3. Sample Source (Linear Interpolation)
    vec4 phi_n_1 = texture(texSource, srcUV);

    // --- MACCORMACK CORRECTION ---
    
    // 4. Advect Backward (where would it go?)
    vec2 vel_prev = texture(texVelocity, srcUV).xy;
    vec2 backUV = srcUV + (vel_prev * dt * texelSize);

    // 5. Sample Current State at the "back" position
    vec4 phi_n = texture(texSource, backUV);
    
    // 6. Calculate Error: (Original - Backtraced)
    vec4 currentVal = texture(texSource, uv);
    vec4 error = (phi_n - currentVal) * 0.5;

    // 7. Corrected Result
    vec4 result = phi_n_1 - error;

    // --- LIMITER (THE NOISE FIX) ---
    // The correction above can produce values outside the valid range (noise).
    // We clamp the result to the min/max of the 4 neighbors of the source pixel.
    
    vec4 s0 = texture(texSource, srcUV + vec2(-texelSize.x, 0.0));
    vec4 s1 = texture(texSource, srcUV + vec2( texelSize.x, 0.0));
    vec4 s2 = texture(texSource, srcUV + vec2(0.0, -texelSize.y));
    vec4 s3 = texture(texSource, srcUV + vec2(0.0,  texelSize.y));
    
    vec4 minVal = min(min(s0, s1), min(s2, s3));
    vec4 maxVal = max(max(s0, s1), max(s2, s3));
    
    result = clamp(result, minVal, maxVal);

    // 8. Decay (very gentle — keep smoke bright)
    result *= 0.9995;

    imageStore(texDest, coords, result);
}
