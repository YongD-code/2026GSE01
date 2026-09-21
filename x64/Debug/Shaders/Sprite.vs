#version 330
uniform vec2 viewport;
uniform vec4 rectangle, uvRect;
out vec2 uv;

void main()
{
    vec2 corners[6] =
        vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
    vec2 p = corners[gl_VertexID];
    vec2 screen = rectangle.xy + p * rectangle.zw;
    uv = mix(uvRect.xy, uvRect.zw, p);
    gl_Position = vec4(screen.x / viewport.x * 2. - 1., 1. - screen.y / viewport.y * 2., 0, 1);
}
