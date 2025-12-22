#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D texVelSource;
layout(binding = 1) uniform sampler2D texObs;
layout(binding = 2) uniform sampler2D texDensSource;

layout(binding = 3, rgba16f) uniform writeonly image2D texVelDest;
layout(binding = 4, r16f)    uniform writeonly image2D texDensDest;

uniform float dt;
uniform float time;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVelDest);
    
    if (coords.x >= size.x || coords.y >= size.y) return;

    vec2 uv = (vec2(coords) + 0.5) / vec2(size);

    if (texture(texObs, uv).r > 0.5) {
        imageStore(texVelDest, coords, vec4(0));
        imageStore(texDensDest, coords, vec4(0));
        return;
    }

    // 1. Advect Velocity
    vec2 vel = texture(texVelSource, uv).xy;
    vec2 oldPos = vec2(coords) - vel * dt;
    vec2 oldUV = (oldPos + 0.5) / vec2(size);
    
    vec4 vNew = texture(texVelSource, oldUV);
    
    // Wind Tunnel Inflow
    if (coords.x < 5) {
        vNew = vec4(400.0, 0.0, 0.0, 0.0); 
    }
    
    vNew *= 0.999;
    imageStore(texVelDest, coords, vNew);

    // 2. Advect Density
    // Check if we are pulling from a wall
    float obsAtSource = texture(texObs, oldUV).r;
    float d = 0.0;

    if (obsAtSource > 0.5) {
        d = texture(texDensSource, uv).r; 
    } else {
        d = texture(texDensSource, oldUV).r;
    }
    
    // --- FIX: SMOOTH CONTINUOUS INJECTION ---
    // 1. Widen the zone to 25px so fast fluid doesn't "jump" over it (Fixes Dots)
    if (coords.x < 25) {
        
        // 2. Use Sine wave instead of Modulo (%) for smooth, organic lines
        // sin(y * 0.2) creates the spacing.
        float rakePattern = sin(coords.y * 0.2); 
        
        // 3. smoothstep makes the lines soft (anti-aliased) instead of blocky
        float intensity = smoothstep(0.8, 0.95, rakePattern); 

        // Restrict to middle height of tunnel
        if (coords.y > 100.0 && coords.y < 412.0) {
             // Use max to ensure we don't cut off existing smoke
             d = max(d, intensity);
        }
    }
    
    d *= 0.9995; 
    imageStore(texDensDest, coords, vec4(d, 0, 0, 0));
}
