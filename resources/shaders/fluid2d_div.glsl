#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// texVel is staggered (W+1)x(H+1): u on left faces, v on bottom faces.
// Divergence uses the same compact stencil as the pressure Laplacian.
layout(binding = 0, rgba32f) uniform readonly image2D texVel;
layout(binding = 1, r32f)    uniform readonly image2D texObs;
layout(binding = 2, r32f)    uniform writeonly image2D texDiv;

bool IsSolidCell(ivec2 c) {
    ivec2 cells = imageSize(texObs);
    if (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, cells))) return false;
    return imageLoad(texObs, c).r > 0.5;
}

float UFlux(ivec2 f) {
    return IsSolidCell(f - ivec2(1, 0)) || IsSolidCell(f) ? 0.0 : imageLoad(texVel, f).x;
}

float VFlux(ivec2 f) {
    int height = imageSize(texObs).y;
    bool closed = f.y == 0 || f.y == height || IsSolidCell(f - ivec2(0, 1)) || IsSolidCell(f);
    return closed ? 0.0 : imageLoad(texVel, f).y;
}

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(texDiv);
    if (coords.x >= size.x || coords.y >= size.y) return;

    float div = 0.0;
    if (!IsSolidCell(coords))
        div = (UFlux(coords + ivec2(1, 0)) - UFlux(coords)) +
              (VFlux(coords + ivec2(0, 1)) - VFlux(coords));
    imageStore(texDiv, coords, vec4(div, 0, 0, 0));
}
