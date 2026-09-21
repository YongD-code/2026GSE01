#version 330
in vec2 uv;
out vec4 result;
uniform sampler2D sourceImage;
uniform vec2 direction;
uniform bool extractBright;
uniform float threshold;

vec3 sampleColor(vec2 p)
{
    vec3 c = texture(sourceImage, p).rgb;
    if (extractBright)
    {
        float brightness = max(c.r, max(c.g, c.b));
        float knee = max(threshold * .5, .001);
        float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
        soft = soft * soft / (4.0 * knee);
        c *= max(brightness - threshold, soft) / max(brightness, .0001);
    }
    return c;
}

void main()
{
    vec2 stepUV = direction / vec2(textureSize(sourceImage, 0));
    vec3 color = sampleColor(uv) * .227027;
    color += (sampleColor(uv + stepUV * 1.384615) + sampleColor(uv - stepUV * 1.384615)) * .316216;
    color += (sampleColor(uv + stepUV * 3.230769) + sampleColor(uv - stepUV * 3.230769)) * .070270;
    result = vec4(color, 1.0);
}
