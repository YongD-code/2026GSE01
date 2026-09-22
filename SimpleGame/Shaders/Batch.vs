#version 330 core
layout(location = 0) in vec4 transform;
layout(location = 1) in vec4 placement;
layout(location = 2) in vec4 color;
layout(location = 3) in vec4 uvRect;
layout(location = 4) in vec4 material;
uniform samplerBuffer meshPool;
uniform vec2 viewport;
out vec4 tint;
out vec2 uv;
flat out int textureSlot;
flat out float emission;

void main()
{
    textureSlot = int(material.x);
    emission = material.y;
    if (gl_VertexID >= int(placement.w))
    {
        gl_Position = vec4(2, 2, 0, 1);
        tint = vec4(0);
        uv = vec2(0);
        return;
    }
    int address = (int(placement.z) + gl_VertexID) * 2;
    vec2 local = texelFetch(meshPool, address).xy;
    vec2 position = mat2(transform.xy, transform.zw) * local + placement.xy;
    gl_Position = vec4(position.x / viewport.x * 2 - 1, 1 - position.y / viewport.y * 2, 0, 1);
    tint = texelFetch(meshPool, address + 1) * color;
    uv = mix(uvRect.xy, uvRect.zw, local);
}
