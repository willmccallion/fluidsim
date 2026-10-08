#version 430

// Sums the fine-grid residual of each 2x2 block into the coarse right-hand
// side (the factor 4 of the coarse h^2 scaling cancels the 1/4 of averaging)
// and zeroes the coarse correction.

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r32f) uniform readonly image2D texFineP;
layout(binding = 1, r32f) uniform readonly image2D texFineRhs;
layout(binding = 2, r32f) uniform readonly image2D texFineMask;
layout(binding = 3, r32f) uniform writeonly image2D texCoarseRhs;
layout(binding = 4, r32f) uniform writeonly image2D texCoarseP;

bool IsSolid(ivec2 c) { return imageLoad(texFineMask, c).r > 0.5; }

void AddNeighbor(ivec2 n, ivec2 size, inout float neighborSum, inout float diagonal) {
    if (n.x >= size.x) {
        diagonal += 2.0;
        return;
    }
    if (n.x < 0 || n.y < 0 || n.y >= size.y || IsSolid(n))
        return;
    neighborSum += imageLoad(texFineP, n).r;
    diagonal += 1.0;
}

float Residual(ivec2 c, ivec2 size) {
    if (IsSolid(c)) return 0.0;

    float neighborSum = 0.0;
    float diagonal = 0.0;
    AddNeighbor(c + ivec2(-1, 0), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2( 1, 0), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2(0, -1), size, neighborSum, diagonal);
    AddNeighbor(c + ivec2(0,  1), size, neighborSum, diagonal);

    float p = imageLoad(texFineP, c).r;
    return imageLoad(texFineRhs, c).r - (neighborSum - diagonal * p);
}

void main() {
    ivec2 coarse = ivec2(gl_GlobalInvocationID.xy);
    ivec2 coarseSize = imageSize(texCoarseRhs);
    if (coarse.x >= coarseSize.x || coarse.y >= coarseSize.y) return;

    ivec2 fineSize = imageSize(texFineP);
    ivec2 fine = coarse * 2;
    float rhs = Residual(fine, fineSize) + Residual(fine + ivec2(1, 0), fineSize) +
                Residual(fine + ivec2(0, 1), fineSize) + Residual(fine + ivec2(1, 1), fineSize);

    imageStore(texCoarseRhs, coarse, vec4(rhs, 0.0, 0.0, 0.0));
    imageStore(texCoarseP, coarse, vec4(0.0));
}
