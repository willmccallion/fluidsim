#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;

void main() {
    float mask = texture(texture0, fragTexCoord).r;
    // Solid dark blue-grey where car exists, fully transparent elsewhere
    if (mask < 0.5) discard;
    finalColor = vec4(0.18, 0.20, 0.28, 1.0);
}
