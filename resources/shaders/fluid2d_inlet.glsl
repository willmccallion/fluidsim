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

float GustNoise(float y, float offset) {
    vec2 p = vec2(y * GUST_BUMPS_PER_HEIGHT, time * GUST_CHANGES_PER_SECOND);
    return ValueNoise(p + vec2(offset, offset * 0.3));
}

void main() {
    // texVel is staggered: (W+1)x(H+1), u on left faces, v on bottom faces.
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 cells = imageSize(texDens);

    // Only run on the Inlet (Left Edge)
    if (coords.x < 10 && coords.y <= cells.y) {
        // 1. Force the inflow velocity, optionally with gusts
        float uFaceY = (float(coords.y) + 0.5) / float(cells.y);
        float vFaceY = float(coords.y) / float(cells.y);
        float u = windSpeed * (1.0 + turbulence * GustNoise(uFaceY, 0.0));
        bool wallFace = coords.y == 0 || coords.y == cells.y;
        float v = wallFace ? 0.0 : windSpeed * turbulence * GustNoise(vFaceY, 37.0);
        imageStore(texVel, coords, vec4(coords.y < cells.y ? u : 0.0, v, 0.0, 0.0));

        // 2. Smoke rake — evenly-spaced streamlines
        float spacing  = cells.y / float(lineCount);
        float dist     = abs(mod(float(coords.y), spacing) - spacing * 0.5);

        // Crisp lines with soft edges
        float smoke = smoothstep(lineHalfWidth * 1.75, lineHalfWidth, dist);

        // Allow lines across 10-90% of height (wider coverage)
        if (coords.y < int(cells.y * 0.10) || coords.y > int(cells.y * 0.90))
            smoke = 0.0;

        // Store brightness in RGB, alpha encodes presence for discard test
        if (coords.y < cells.y)
            imageStore(texDens, coords, vec4(smoke, smoke, smoke, smoke));
    }
}
