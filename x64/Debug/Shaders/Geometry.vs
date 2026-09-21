#version 330
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
out vec4 tint;
uniform vec2 viewport;
uniform vec2 meshOffset;

void main()
{
    vec2 screen = position + meshOffset;
    gl_Position =
        vec4(screen.x / viewport.x * 2.0 - 1.0, 1.0 - screen.y / viewport.y * 2.0, 0.0, 1.0);
    tint = color;
}
