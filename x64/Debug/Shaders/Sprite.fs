#version 330
uniform sampler2D image;
uniform vec4 tint;
uniform bool linearScene;
uniform float emission;
in vec2 uv;
out vec4 result;

void main()
{
    vec4 pixel = texture(image, uv);
    if (pixel.a < .01)
        discard;
    vec3 color = pixel.rgb * tint.rgb;
    vec3 linearColor = pow(max(color, vec3(0)), vec3(2.2));
    float bright = smoothstep(.45, .9, max(color.r, max(color.g, color.b)));
    result = vec4(linearScene ? linearColor * (1.0 + emission * bright) : color, pixel.a * tint.a);
}
