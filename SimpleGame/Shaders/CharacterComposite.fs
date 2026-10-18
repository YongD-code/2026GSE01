#version 330 core
uniform sampler2D characterImage;
uniform vec4 tint;
uniform bool linearScene;
in vec2 uv;
out vec4 outputColor;

void main()
{
    // Multisample resolve produces premultiplied edge coverage over transparent black.
    vec4 pixel = texture(characterImage, uv);
    if (pixel.a < .001)
        discard;
    vec3 rgb = pixel.rgb / pixel.a * tint.rgb;
    if (!linearScene)
        rgb = pow(max(rgb, vec3(0)), vec3(1.0 / 2.2));
    float alpha = pixel.a * tint.a;
    outputColor = vec4(rgb * alpha, alpha);
}
