#version 330
in vec4 tint;
out vec4 outputColor;
uniform bool linearScene;

void main()
{
    outputColor = vec4(linearScene ? pow(max(tint.rgb, vec3(0)), vec3(2.2)) : tint.rgb, tint.a);
}
