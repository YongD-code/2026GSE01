#version 330
in vec2 uv;
out vec4 result;
uniform sampler2D sceneImage, blurImage, bloomImage, broadBloomImage;
uniform float exposure, bloomStrength, vignetteStrength, edgeBlurStrength;

vec3 toneMap(vec3 c)
{
    return clamp((c * (2.51 * c + .03)) / (c * (2.43 * c + .59) + .14), 0.0, 1.0);
}

void main()
{
    // Elliptical screen-space falloff: clear center, gradually softer/darker edges.
    float radius = length((uv - .5) * 2.0);
    float edge = smoothstep(.45, 1.25, radius);
    vec3 color = mix(texture(sceneImage, uv).rgb,
                     texture(blurImage, uv).rgb,
                     clamp(edge * edgeBlurStrength, 0.0, 1.0));
    // Retain a tight halo while adding a softer, wider scattering component.
    vec3 bloom = texture(bloomImage, uv).rgb * .4 + texture(broadBloomImage, uv).rgb * .6;
    color += bloom * bloomStrength;
    color *= 1.0 - clamp(vignetteStrength, 0.0, .9) * smoothstep(.35, 1.35, radius);
    color = toneMap(color * max(exposure, .01));
    result = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
