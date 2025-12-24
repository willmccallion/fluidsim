#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// We read from the texture, add value, and write back to the same texture
layout(binding = 0, rgba16f) uniform image2D targetTex;

uniform vec2 point;      // Location in texture coordinates
uniform float radius;    // Radius in pixels
uniform vec4 color;      // Value to add (Density color or Velocity vector)

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(targetTex);

    if (coords.x >= size.x || coords.y >= size.y) return;

    // Calculate distance from splat center
    float dist = distance(vec2(coords), point);

    if (dist < radius) {
        // Gaussian falloff
        float intensity = exp(-dist * dist / (radius * 0.5));
        
        vec4 pixel = imageLoad(targetTex, coords);
        pixel += color * intensity;
        imageStore(targetTex, coords, pixel);
    }
}
