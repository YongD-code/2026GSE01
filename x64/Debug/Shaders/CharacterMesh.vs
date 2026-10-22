#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec4 jointIndices;
layout(location = 4) in vec4 weights;
uniform mat4 bones[32];
uniform vec2 heading;
uniform float modelScale;
out vec3 surfaceNormal;
out vec4 baseColor;
out vec3 worldPosition;

void main()
{
    ivec4 joint = ivec4(jointIndices);
    mat4 skin = bones[joint.x] * weights.x + bones[joint.y] * weights.y +
                bones[joint.z] * weights.z + bones[joint.w] * weights.w;
    vec3 p = (skin * vec4(position, 1)).xyz;
    vec3 n = mat3(skin) * normal;
    mat2 rotation = mat2(heading.x, heading.y, -heading.y, heading.x);
    p.xy = rotation * p.xy;
    n.xy = rotation * n.xy;
    // Match Prototype::Screen's isometric basis. The model is 90 world units tall.
    worldPosition = p * 90.0;
    vec2 screen =
        vec2(128, 280) + vec2(.85 * (worldPosition.x - worldPosition.y),
                              .43 * (worldPosition.x + worldPosition.y) - worldPosition.z) *
                             2.0 * modelScale;
    float depth = -dot(worldPosition, vec3(.603, .603, .519)) * modelScale / 180.0;
    gl_Position = vec4(screen.x / 128.0 - 1, 1 - screen.y / 160.0, depth, 1);
    surfaceNormal = n;
    baseColor = color;
}
