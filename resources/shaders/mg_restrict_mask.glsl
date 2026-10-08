#version 430

// A coarse cell is solid only when all four of its fine children are solid.

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r32f) uniform readonly image2D texFineMask;
layout(binding = 1, r32f) uniform writeonly image2D texCoarseMask;

void main() {
    ivec2 coarse = ivec2(gl_GlobalInvocationID.xy);
    ivec2 coarseSize = imageSize(texCoarseMask);
    if (coarse.x >= coarseSize.x || coarse.y >= coarseSize.y) return;

    ivec2 fine = coarse * 2;
    float solid = min(min(imageLoad(texFineMask, fine).r,
                          imageLoad(texFineMask, fine + ivec2(1, 0)).r),
                      min(imageLoad(texFineMask, fine + ivec2(0, 1)).r,
                          imageLoad(texFineMask, fine + ivec2(1, 1)).r));
    imageStore(texCoarseMask, coarse, vec4(solid, 0.0, 0.0, 0.0));
}
