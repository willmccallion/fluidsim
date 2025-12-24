#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0; // Smoke
uniform sampler2D texture1; // Data
uniform vec4 colDiffuse;
uniform int mode; 

// NEW: Auto-Range Uniforms
uniform float maxPressure;
uniform float maxVelocity;

vec3 getSpectrum(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 col = vec3(0.0);
    if (t < 0.25) col = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), t * 4.0);
    else if (t < 0.5) col = mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);
    else if (t < 0.75) col = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);
    else col = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);
    return col;
}

void main() {
    vec4 dense = texture(texture0, fragTexCoord);
    if (dense.a < 0.05) discard;

    vec3 finalRGB = dense.rgb;

    if (mode == 1) {
        // --- AUTO-RANGED PRESSURE ---
        float p = texture(texture1, fragTexCoord).r;
        
        // Normalize: p / maxPressure
        // Since p can be negative, we map [-max, +max] to [0, 1]
        // 0.5 is neutral.
        float norm = (p / maxPressure) * 0.5 + 0.5;
        
        finalRGB = getSpectrum(norm) * length(dense.rgb) * 1.2;
    } 
    else if (mode == 2) {
        // --- AUTO-RANGED VELOCITY ---
        float speed = length(texture(texture1, fragTexCoord).xy);
        
        // Normalize: speed / maxVelocity
        float norm = speed / maxVelocity;
        
        finalRGB = getSpectrum(norm) * length(dense.rgb) * 1.2;
    }

    finalColor = vec4(finalRGB, dense.a);
}
