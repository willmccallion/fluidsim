#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba16f) uniform image2D texVel;
layout(binding = 1, rgba16f) uniform image2D texDens;

uniform float time;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);

    // Only run on the Inlet (Left Edge)
    if (coords.x < 10) {
        // 1. Force High Velocity
        imageStore(texVel, coords, vec4(1200.0, 0.0, 0.0, 0.0));

        // 2. The Smoke "Rake" (Streamlines)
        float numLines = 16.0;
        float spacing = size.y / numLines;
        
        // Calculate distance to the center of the nearest line
        float dist = abs(mod(coords.y, spacing) - (spacing * 0.5));
        
        // --- THICKER LINES ---
        // Previous was smoothstep(2.5, 1.5, dist)
        // Now we use 8.0 to 6.0, making the lines about 16 pixels wide total
        // with a soft edge so they don't look pixelated.
        float smoke = smoothstep(9.0, 6.0, dist);

        // Restrict vertical area
        if (coords.y < size.y * 0.15 || coords.y > size.y * 0.85) {
            smoke = 0.0;
        }

        // 3. Output Color
        // Pure white smoke, modulated by the shape
        vec3 smokeColor = vec3(0.95, 0.95, 0.95); 
        imageStore(texDens, coords, vec4(smokeColor * smoke, 1.0));
    }
}
