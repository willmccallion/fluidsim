#version 430

// Adds the bilinearly interpolated coarse correction to the fine pressure.
// Solid coarse cells are skipped and the remaining weights renormalised.

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, r32f) uniform readonly image2D texCoarseP;
layout(binding = 1, r32f) uniform readonly image2D texCoarseMask;
layout(binding = 2, r32f) uniform image2D texFineP;
layout(binding = 3, r32f) uniform readonly image2D texFineMask;

void main() {
    ivec2 fine = ivec2(gl_GlobalInvocationID.xy);
    ivec2 fineSize = imageSize(texFineP);
    if (fine.x >= fineSize.x || fine.y >= fineSize.y) return;
    if (imageLoad(texFineMask, fine).r > 0.5) return;

    ivec2 coarseSize = imageSize(texCoarseP);
    vec2 coarsePos = (vec2(fine) + 0.5) * 0.5 - 0.5;
    ivec2 base = ivec2(floor(coarsePos));
    vec2 f = coarsePos - vec2(base);

    float correction = 0.0;
    float weightSum = 0.0;
    for (int j = 0; j <= 1; j++) {
        for (int i = 0; i <= 1; i++) {
            ivec2 c = clamp(base + ivec2(i, j), ivec2(0), coarseSize - 1);
            if (imageLoad(texCoarseMask, c).r > 0.5) continue;
            float w = (i == 0 ? 1.0 - f.x : f.x) * (j == 0 ? 1.0 - f.y : f.y);
            correction += w * imageLoad(texCoarseP, c).r;
            weightSum += w;
        }
    }
    if (weightSum <= 0.0) return;

    float p = imageLoad(texFineP, fine).r + correction / weightSum;
    imageStore(texFineP, fine, vec4(p, 0.0, 0.0, 0.0));
}
