#version 330 core
uniform vec2 viewport, feet;
uniform float drawScale;
uniform vec2 imageAnchor;
out vec2 uv;

void main()
{
    vec2 points[6] =
        vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = points[gl_VertexID];
    vec2 screen = feet + (corner * vec2(128, 160) - imageAnchor) * drawScale;
    gl_Position = vec4(screen.x / viewport.x * 2 - 1, 1 - screen.y / viewport.y * 2, 0, 1);
    uv = vec2(corner.x, 1 - corner.y);
}
