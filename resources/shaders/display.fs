#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0; // Smoke
uniform sampler2D texture1; // Data
uniform vec4 colDiffuse;
uniform int mode;

uniform float maxPressure;
uniform float maxVelocity;
uniform float maxCurl;

vec3 getSpectrum(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 col = vec3(0.0);
    if (t < 0.25) col = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), t * 4.0);
    else if (t < 0.5) col = mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);
    else if (t < 0.75) col = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);
    else col = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);
    return col;
}

// Bipolar blue->black->red colormap for signed curl
vec3 getCurlColor(float t) {
    t = clamp(t, -1.0, 1.0);
    if (t < 0.0) return mix(vec3(0.0), vec3(0.1, 0.4, 1.0), -t);
    else          return mix(vec3(0.0), vec3(1.0, 0.2, 0.05), t);
}

void main() {
    vec4 dense = texture(texture0, fragTexCoord);

    if (mode == 3) {
        // Curl mode: show vorticity field directly, independent of dye
        float curl = texture(texture1, fragTexCoord).r;
        float norm = curl / max(maxCurl, 0.0001);
        float mag = abs(norm);
        vec3 col = getCurlColor(norm);
        // Blend with dye for regions where fluid is visible
        float dyeBrightness = length(dense.rgb);
        col = mix(col * 0.4, col, smoothstep(0.0, 0.3, mag));
        col += dense.rgb * 0.15; // subtle dye bleed-through
        finalColor = vec4(col, max(mag * 1.5, dyeBrightness * 0.5 + 0.05));
        return;
    }

    if (dense.a < 0.05) discard;

    vec3 finalRGB = dense.rgb;

    if (mode == 1) {
        float p = texture(texture1, fragTexCoord).r;
        float norm = (p / maxPressure) * 0.5 + 0.5;
        finalRGB = getSpectrum(norm) * length(dense.rgb) * 1.2;
    }
    else if (mode == 2) {
        float speed = length(texture(texture1, fragTexCoord).xy);
        float norm = speed / maxVelocity;
        finalRGB = getSpectrum(norm) * length(dense.rgb) * 1.2;
    }

    finalColor = vec4(finalRGB, dense.a);
}
