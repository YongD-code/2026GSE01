#version 330 core
in vec3 surfaceNormal;
in vec4 baseColor;
in vec2 uv;
in vec3 worldPosition;
uniform sampler2D baseTexture;
uniform bool hasTexture, unlit, fourBand;
uniform vec4 materialColor;
uniform vec3 emission;
uniform int alphaMode;
uniform float alphaCutoff;
out vec4 outputColor;

void main()
{
    vec4 albedo = materialColor * baseColor;
    if (hasTexture)
        albedo *= texture(baseTexture, uv);
    if (alphaMode == 1 && albedo.a < alphaCutoff)
        discard;
    float alpha = alphaMode == 2 ? clamp(albedo.a, 0.0, 1.0) : 1.0;
    if (alpha < .001)
        discard;
    vec3 n = normalize(surfaceNormal);
    if (!gl_FrontFacing)
        n = -n;
    float diffuse = dot(n, normalize(vec3(-.45, -.65, 1.1)));
    float bands = fourBand ? .20 + .20 * smoothstep(.15, .17, diffuse) +
                                .28 * smoothstep(.39, .41, diffuse) +
                                .32 * smoothstep(.71, .73, diffuse)
                          : .48 + .22 * smoothstep(-.08, .06, diffuse) +
                                .28 * smoothstep(.48, .60, diffuse);
    float contact = mix(.86, 1.0, smoothstep(0.0, 25.0, worldPosition.z));
    vec3 rgb = albedo.rgb * (unlit ? 1.0 : (bands + .04 * max(n.z, 0.0)) * contact) + emission;
    outputColor = vec4(rgb * alpha, alpha);
}
