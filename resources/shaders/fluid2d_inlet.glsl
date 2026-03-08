#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba16f) uniform image2D texVel;
layout(binding = 1, rgba16f) uniform image2D texDens;

uniform float time;
uniform float windSpeed;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);

    // Only run on the Inlet (Left Edge)
    if (coords.x < 10) {
        // 1. Force uniform velocity
        imageStore(texVel, coords, vec4(windSpeed, 0.0, 0.0, 0.0));

        // 2. Smoke rake — 20 evenly-spaced streamlines
        float numLines = 20.0;
        float spacing  = size.y / numLines;
        float dist     = abs(mod(float(coords.y), spacing) - spacing * 0.5);

        // Thick crisp lines with soft edges
        float smoke = smoothstep(7.0, 4.0, dist);

        // Allow lines across 10-90% of height (wider coverage)
        if (coords.y < int(size.y * 0.10) || coords.y > int(size.y * 0.90))
            smoke = 0.0;

        // Store brightness in RGB, alpha encodes presence for discard test
        imageStore(texDens, coords, vec4(smoke, smoke, smoke, smoke));
    }
}
