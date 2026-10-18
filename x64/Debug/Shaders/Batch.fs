#version 330 core
in vec4 tint;
in vec2 uv;
flat in vec4 imageBounds;
flat in int textureSlot;
flat in float emission;
flat in vec2 energy;
uniform bool linearScene;
uniform sampler2D image0, image1, image2, image3, image4, image5, image6, image7;
out vec4 outputColor;

float energyNoise(vec2 p)
{
    vec2 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec4 k = vec4(dot(cell, vec2(127.1, 311.7)),
                  dot(cell + vec2(1, 0), vec2(127.1, 311.7)),
                  dot(cell + vec2(0, 1), vec2(127.1, 311.7)),
                  dot(cell + vec2(1, 1), vec2(127.1, 311.7)));
    vec4 h = fract(sin(k) * 43758.5453);
    return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y);
}

void main()
{
    vec4 pixel = vec4(1);
    vec2 sampleUV = uv;
    float flowNoise = 1.0;
    if (energy.y > 3.5)
    {
        vec2 extent = max(imageBounds.zw - imageBounds.xy, vec2(.0001));
        vec2 local = (uv - imageBounds.xy) / extent;
        vec2 p = local * 2.0 - 1.0;
        float radial = length(p);
        if (radial > 1.0)
            discard;
        // Low-cost edge refraction leaves the interior architecture readable.
        float edge = smoothstep(.55, 1.0, radial);
        float ripple =
            sin(p.x * 17.0 + abs(energy.x) * 2.0) * sin(p.y * 13.0 - abs(energy.x) * 1.4);
        sampleUV = clamp(uv + p * ripple * extent * edge * .008, imageBounds.xy, imageBounds.zw);
    }
    else if (energy.y > 2.5)
    {
        vec2 extent = max(imageBounds.zw - imageBounds.xy, vec2(.0001));
        vec2 local = (uv - imageBounds.xy) / extent;
        float edge =
            smoothstep(0.0, .18, min(min(local.x, local.y), min(1 - local.x, 1 - local.y)));
        vec2 drift = vec2(energy.x * .35, -energy.x * .7);
        vec2 warp = vec2(energyNoise(local * 5 + drift), energyNoise(local * 5 - drift + 7)) - .5;
        sampleUV = clamp(uv + warp * extent * .055 * edge, imageBounds.xy, imageBounds.zw);
        flowNoise = energyNoise(local * 9 + drift);
    }
    if (textureSlot == 0)
        pixel = texture(image0, sampleUV);
    else if (textureSlot == 1)
        pixel = texture(image1, sampleUV);
    else if (textureSlot == 2)
        pixel = texture(image2, sampleUV);
    else if (textureSlot == 3)
        pixel = texture(image3, sampleUV);
    else if (textureSlot == 4)
        pixel = texture(image4, sampleUV);
    else if (textureSlot == 5)
        pixel = texture(image5, sampleUV);
    else if (textureSlot == 6)
        pixel = texture(image6, sampleUV);
    else if (textureSlot == 7)
        pixel = texture(image7, sampleUV);
    if (textureSlot >= 0 && pixel.a < .01)
        discard;
    if (energy.y > 3.5)
    {
        vec2 extent = max(imageBounds.zw - imageBounds.xy, vec2(.0001));
        vec2 p = (uv - imageBounds.xy) / extent * 2.0 - 1.0;
        float radial = length(p);
        float edge = 1.0 - smoothstep(.975, 1.0, radial);
        if (energy.x < 0.0)
        {
            float collapse = clamp(-energy.x - 1.0, 0.0, 1.0);
            float fragments = energyNoise(floor(p * 24.0));
            edge *= smoothstep(collapse - .06, collapse + .06, fragments);
        }
        pixel.a *= edge;
        pixel.rgb *= 1.0 - smoothstep(.55, 1.0, radial) * .35;
        if (pixel.a * tint.a < .003)
            discard;
    }
    else if (energy.y > 2.5)
    {
        // Fade by dissolving into uneven wisps instead of fading a rectangular sticker.
        float threshold = (1.0 - clamp(tint.a, 0.0, 1.0)) * .85;
        float reveal = smoothstep(threshold - .12, threshold + .12, flowNoise);
        float rim = 1.0 - smoothstep(.02, .14, abs(flowNoise - threshold));
        pixel.a *= reveal;
        pixel.rgb *= 1.0 + rim * .4 + .12 * sin(energy.x * 8 + flowNoise * 12);
    }
    else if (energy.y > .5)
    {
        vec2 p = uv * 2.0 - 1.0;
        float radial = dot(p, p);
        float mask = exp(-radial * 4.5) * (1.0 - smoothstep(.55, 1.0, radial));
        if (energy.y > 1.5)
        {
            vec2 flow = vec2(energy.x * .7, -energy.x * 1.1);
            float n = energyNoise(p * 3.0 + flow);
            n = n * .65 + energyNoise(p * 7.0 - flow * .6) * .35;
            mask *= smoothstep(.15, .75, n) * 1.8;
            pixel.rgb = mix(vec3(.35, .12, .15), vec3(1.0), n);
        }
        pixel.a *= clamp(mask, 0.0, 1.0);
        if (pixel.a * tint.a < .003)
            discard;
    }
    vec3 rgb = pixel.rgb * tint.rgb;
    float bright = smoothstep(.45, .9, max(rgb.r, max(rgb.g, rgb.b)));
    if (linearScene)
        rgb = pow(max(rgb, vec3(0)), vec3(2.2)) * (1 + emission * bright);
    outputColor = vec4(rgb, pixel.a * tint.a);
}
