#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba32f) uniform image2D texVel;
layout(binding = 1, rgba32f) uniform image2D texDens;

uniform float time;
uniform float windSpeed;
uniform int lineCount;
uniform float lineHalfWidth;
uniform float turbulence; // gust strength as a fraction of windSpeed
uniform float seed;

// Gust pattern: bumps across the inlet height and changes per second.
const float GUST_BUMPS_PER_HEIGHT = 16.0;
const float GUST_CHANGES_PER_SECOND = 6.0;

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.13) * 43758.5453);
}

// Smooth value noise in [-1, 1].
float ValueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float bottom = mix(Hash(i), Hash(i + vec2(1.0, 0.0)), u.x);
    float top = mix(Hash(i + vec2(0.0, 1.0)), Hash(i + vec2(1.0, 1.0)), u.x);
    return mix(bottom, top, u.y) * 2.0 - 1.0;
}

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);

    // Only run on the Inlet (Left Edge)
    if (coords.x < 10) {
        // 1. Force the inflow velocity, optionally with gusts
        vec2 gustPos = vec2(float(coords.y) / float(size.y) * GUST_BUMPS_PER_HEIGHT,
                            time * GUST_CHANGES_PER_SECOND);
        vec2 gust = vec2(ValueNoise(gustPos), ValueNoise(gustPos + vec2(37.0, 11.0)));
        vec2 inflow = windSpeed * (vec2(1.0, 0.0) + turbulence * gust);
        imageStore(texVel, coords, vec4(inflow, 0.0, 0.0));

        // 2. Smoke rake — evenly-spaced streamlines
        float spacing  = size.y / float(lineCount);
        float dist     = abs(mod(float(coords.y), spacing) - spacing * 0.5);

        // Crisp lines with soft edges
        float smoke = smoothstep(lineHalfWidth * 1.75, lineHalfWidth, dist);

        // Allow lines across 10-90% of height (wider coverage)
        if (coords.y < int(size.y * 0.10) || coords.y > int(size.y * 0.90))
            smoke = 0.0;

        // Store brightness in RGB, alpha encodes presence for discard test
        imageStore(texDens, coords, vec4(smoke, smoke, smoke, smoke));
    }
}
