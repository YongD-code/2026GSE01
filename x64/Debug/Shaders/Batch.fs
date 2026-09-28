#version 330 core
in vec4 tint;
in vec2 uv;
flat in int textureSlot;
flat in float emission;
uniform bool linearScene;
uniform sampler2D image0, image1, image2, image3, image4, image5, image6, image7;
out vec4 outputColor;

void main()
{
    vec4 pixel = vec4(1);
    if (textureSlot == 0)
        pixel = texture(image0, uv);
    else if (textureSlot == 1)
        pixel = texture(image1, uv);
    else if (textureSlot == 2)
        pixel = texture(image2, uv);
    else if (textureSlot == 3)
        pixel = texture(image3, uv);
    else if (textureSlot == 4)
        pixel = texture(image4, uv);
    else if (textureSlot == 5)
        pixel = texture(image5, uv);
    else if (textureSlot == 6)
        pixel = texture(image6, uv);
    else if (textureSlot == 7)
        pixel = texture(image7, uv);
    if (textureSlot >= 0 && pixel.a < .01)
        discard;
    vec3 rgb = pixel.rgb * tint.rgb;
    float bright = smoothstep(.45, .9, max(rgb.r, max(rgb.g, rgb.b)));
    if (linearScene)
        rgb = pow(max(rgb, vec3(0)), vec3(2.2)) * (1 + emission * bright);
    outputColor = vec4(rgb, pixel.a * tint.a);
}
