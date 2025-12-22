#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;

void main() {
    // Sample the Red channel (Density)
    float d = texture(texture0, fragTexCoord).r;
    
    // Output as Grayscale (R=d, G=d, B=d)
    // Multiply by fragColor to allow tinting if needed
    finalColor = vec4(d, d, d, 1.0) * fragColor;
}
