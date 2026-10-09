#version 430
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    vec2 pos;
    vec2 vel;
    float life;
    float padding;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(binding = 0) uniform sampler2D texVelocity; // staggered (W+1)x(H+1)

uniform float dt;
uniform vec2 res;
uniform float time;

// Texel (i, j) holds u on the left face of cell (i, j) in .x and v on its
// bottom face in .y. Points are in cell units.
vec2 SampleVelocity(vec2 p) {
    vec2 size = vec2(textureSize(texVelocity, 0));
    float u = texture(texVelocity, (p + vec2(0.5, 0.0)) / size).x;
    float v = texture(texVelocity, (p + vec2(0.0, 0.5)) / size).y;
    return vec2(u, v);
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    Particle p = particles[id];

    if (p.life <= 0.0) {
        // Respawn randomly near the inlet or center
        if (rand(vec2(time, id)) > 0.95) {
            p.pos = vec2(10.0, res.y * 0.5 + (rand(vec2(id, time)) - 0.5) * 200.0);
            p.life = 1.0 + rand(vec2(id, time));
        }
    } else {
        // Sample Velocity
        vec2 vel = SampleVelocity(p.pos);
        
        p.pos += vel * dt;
        p.life -= dt * 0.5;
        
        // Kill if out of bounds
        if (p.pos.x > res.x || p.pos.y > res.y || p.pos.y < 0.0) {
            p.life = -1.0;
        }
    }

    particles[id] = p;
}
