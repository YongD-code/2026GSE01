#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec4 jointIndices;
layout(location = 4) in vec4 weights;
layout(location = 5) in vec2 texcoord;
layout(location = 6) in vec3 morphPosition0;
layout(location = 7) in vec3 morphPosition1;
layout(location = 8) in vec3 morphPosition2;
layout(location = 9) in vec3 morphPosition3;
layout(location = 10) in vec3 morphNormal0;
layout(location = 11) in vec3 morphNormal1;
layout(location = 12) in vec3 morphNormal2;
layout(location = 13) in vec3 morphNormal3;
layout(std140) uniform JointPalette
{
    mat4 bones[128];
};
uniform mat4 objectTransform, normalization;
uniform bool skinned;
uniform vec2 heading;
uniform float modelScale;
uniform vec4 morphWeights;
out vec3 surfaceNormal;
out vec4 baseColor;
out vec2 uv;
out vec3 worldPosition;

void main()
{
    ivec4 joint = ivec4(jointIndices);
    mat4 skin = skinned ? bones[joint.x] * weights.x + bones[joint.y] * weights.y +
                             bones[joint.z] * weights.z + bones[joint.w] * weights.w
                       : objectTransform;
    mat4 model = normalization * skin;
    vec3 posed = position + morphPosition0 * morphWeights.x + morphPosition1 * morphWeights.y +
                 morphPosition2 * morphWeights.z + morphPosition3 * morphWeights.w;
    vec3 posedNormal = normal + morphNormal0 * morphWeights.x + morphNormal1 * morphWeights.y +
                       morphNormal2 * morphWeights.z + morphNormal3 * morphWeights.w;
    vec3 p = (model * vec4(posed, 1)).xyz;
    mat3 basis = mat3(model);
    vec3 n = abs(determinant(basis)) > 1e-12 ? transpose(inverse(basis)) * posedNormal : posedNormal;
    mat2 rotation = mat2(heading.x, heading.y, -heading.y, heading.x);
    p.xy = rotation * p.xy;
    n.xy = rotation * n.xy;
    worldPosition = p * 90.0;
    // Leave ground-plane room below wide quadrupeds and long tails.
    vec2 screen = vec2(128, 240) +
                  vec2(.85 * (worldPosition.x - worldPosition.y),
                       .43 * (worldPosition.x + worldPosition.y) - worldPosition.z) *
                      2.0 * modelScale;
    float depth = -dot(worldPosition, vec3(.603, .603, .519)) * modelScale / 180.0;
    gl_Position = vec4(screen.x / 128.0 - 1, 1 - screen.y / 160.0, depth, 1);
    surfaceNormal = n;
    baseColor = color;
    uv = texcoord;
}
