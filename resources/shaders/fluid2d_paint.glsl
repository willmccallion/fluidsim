#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// We bind the obstacle texture as R16F (single channel red)
layout(binding = 0, r16f) uniform image2D targetTex;

uniform vec2 point;
uniform float radius;
uniform float value; // 1.0 for wall, 0.0 for erase

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(targetTex);

    if (coords.x >= size.x || coords.y >= size.y) return;

    // Simple hard circle check
    if (distance(vec2(coords), point) < radius) {
        imageStore(targetTex, coords, vec4(value, 0, 0, 0));
    }
}
