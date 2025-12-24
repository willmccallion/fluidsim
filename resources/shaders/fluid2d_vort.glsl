#version 430
layout (local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba16f) uniform image2D texVel;
layout(binding = 1, r16f) uniform readonly image2D texCurl;

uniform float dt;
uniform float curlStrength;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texVel);

    if (coords.x >= size.x || coords.y >= size.y) return;

    ivec2 L = clamp(coords - ivec2(1,0), ivec2(0), size - 1);
    ivec2 R = clamp(coords + ivec2(1,0), ivec2(0), size - 1);
    ivec2 D = clamp(coords - ivec2(0,1), ivec2(0), size - 1);
    ivec2 U = clamp(coords + ivec2(0,1), ivec2(0), size - 1);

    float cC = imageLoad(texCurl, coords).r;
    float cL = imageLoad(texCurl, L).r;
    float cR = imageLoad(texCurl, R).r;
    float cD = imageLoad(texCurl, D).r;
    float cU = imageLoad(texCurl, U).r;

    // Calculate gradient of the magnitude of curl
    float dC_dx = abs(cR) - abs(cL);
    float dC_dy = abs(cU) - abs(cD);
    
    vec2 forceDir = vec2(dC_dx, dC_dy);
    float len = length(forceDir);

    if (len > 0.0001) {
        forceDir = forceDir / len;
        // The force is perpendicular to the gradient
        // Force = (N x w) -> In 2D: (dir.y, -dir.x) * curlSign
        vec2 force = vec2(forceDir.y, -forceDir.x) * cC * curlStrength;
        
        vec4 velocity = imageLoad(texVel, coords);
        velocity.xy += force * dt;
        imageStore(texVel, coords, velocity);
    }
}
