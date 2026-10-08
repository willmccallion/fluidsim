#version 430

// Red-black Gauss-Seidel sweep over one colour of the pressure Poisson system.
// Walls and obstacles are Neumann (zero gradient). The outlet is p = 0 on the
// right face itself (ghost = -p), so every multigrid level sees the same edge.

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r32f) uniform image2D texP;
layout(binding = 1, r32f) uniform readonly image2D texRhs;
layout(binding = 2, r32f) uniform readonly image2D texMask;

uniform int parity;

bool IsSolid(ivec2 c) { return imageLoad(texMask, c).r > 0.5; }

void AddNeighbor(ivec2 n, ivec2 size, inout float neighborSum, inout float diagonal) {
    if (n.x >= size.x) {
        diagonal += 2.0;
        return;
    }
    if (n.x < 0 || n.y < 0 || n.y >= size.y || IsSolid(n))
        return;
    neighborSum += imageLoad(texP, n).r;
    diagonal += 1.0;
}

void main() {
    ivec2 size = imageSize(texP);
    int y = int(gl_GlobalInvocationID.y);
    int x = 2 * int(gl_GlobalInvocationID.x) + ((y + parity) & 1);
    ivec2 c = ivec2(x, y);
    if (x >= size.x || y >= size.y) return;

    if (IsSolid(c)) {
        imageStore(texP, c, vec4(0.0));
        return;
    }

    float neighborSum = 0.0;
    float diagonal = 0.0;
    AddNeighbor(c + ivec2(-1, 0), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2( 1, 0), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2(0, -1), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2(0,  1), size, neighborSum, diagonal);

    float p = diagonal > 0.0 ? (neighborSum - imageLoad(texRhs, c).r) / diagonal : 0.0;
    imageStore(texP, c, vec4(p, 0.0, 0.0, 0.0));
}
