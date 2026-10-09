#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0; // Smoke/density
uniform sampler2D texture1; // Data (pressure / velocity / curl); velocity is staggered
uniform vec4 colDiffuse;
uniform int mode;

uniform float maxPressure;
uniform float maxVelocity;
uniform float maxCurl;

vec3 getSpectrum(float t) {
    t = clamp(t, 0.0, 1.0);
    if (t < 0.25) return mix(vec3(0.0, 0.0, 0.8), vec3(0.0, 0.8, 1.0), t * 4.0);
    if (t < 0.5)  return mix(vec3(0.0, 0.8, 1.0), vec3(0.0, 0.9, 0.0), (t - 0.25) * 4.0);
    if (t < 0.75) return mix(vec3(0.0, 0.9, 0.0), vec3(1.0, 0.9, 0.0), (t - 0.5)  * 4.0);
                  return mix(vec3(1.0, 0.9, 0.0), vec3(1.0, 0.1, 0.0), (t - 0.75) * 4.0);
}

vec3 getCurlColor(float t) {
    t = clamp(t, -1.0, 1.0);
    if (t < 0.0) return mix(vec3(0.0), vec3(0.1, 0.4, 1.0), -t);
    else         return mix(vec3(0.0), vec3(1.0, 0.2, 0.05), t);
}

// Staggered velocity: texel (i, j) holds u on the left face of cell (i, j)
// in .x and v on its bottom face in .y. Points are in cell units.
vec2 SampleVelocity(vec2 p) {
    vec2 size = vec2(textureSize(texture1, 0));
    float u = texture(texture1, (p + vec2(0.5, 0.0)) / size).x;
    float v = texture(texture1, (p + vec2(0.0, 0.5)) / size).y;
    return vec2(u, v);
}

void main() {
    vec4 dense = texture(texture0, fragTexCoord);

    // --- Mode 3: Vorticity (full-field, no smoke dependency) ---
    if (mode == 3) {
        float curl = texture(texture1, fragTexCoord).r;
        float norm = curl / max(maxCurl, 0.0001);
        float mag  = abs(norm);
        vec3 col   = getCurlColor(norm);
        col = mix(col * 0.3, col, smoothstep(0.0, 0.25, mag));
        col += dense.rgb * 0.2;
        finalColor = vec4(col * 1.5, max(mag * 2.0, length(dense.rgb) * 0.4 + 0.05));
        return;
    }

    // --- Mode 1: Pressure ---
    if (mode == 1) {
        float p    = texture(texture1, fragTexCoord).r;
        float norm = (p / max(maxPressure, 0.0001)) * 0.5 + 0.5;
        vec3  col  = getSpectrum(norm);
        // Show full field — use smoke alpha as mask but render everywhere fluid exists
        float smoke = clamp(length(dense.rgb) * 2.0, 0.0, 1.0);
        finalColor = vec4(col, max(smoke, 0.06));
        return;
    }

    // --- Mode 2: Velocity ---
    if (mode == 2) {
        vec2 cells = vec2(textureSize(texture0, 0));
        float speed = length(SampleVelocity(fragTexCoord * cells));
        float norm  = clamp(speed / max(maxVelocity, 0.0001), 0.0, 1.0);
        vec3  col   = getSpectrum(norm);
        float smoke = clamp(length(dense.rgb) * 2.0, 0.0, 1.0);
        finalColor = vec4(col, max(smoke, 0.06));
        return;
    }

    // --- Mode 0: RGB Smoke ---
    if (dense.a < 0.04) discard;
    // Boost brightness so smoke is vivid
    vec3 rgb = dense.rgb * 2.2;
    finalColor = vec4(rgb, dense.a);
}
