#version 330 core
in vec3 surfaceNormal;
in vec4 baseColor;
in vec3 worldPosition;
uniform bool fourBand;
out vec4 outputColor;

void main()
{
    if (baseColor.a < .01)
        discard;
    vec3 n = normalize(surfaceNormal);
    if (!gl_FrontFacing)
        n = -n;
    vec3 light = normalize(vec3(-.45, -.65, 1.1));
    float diffuse = dot(n, light);
    float bands = .48 + .22 * smoothstep(-.08, .06, diffuse) + .28 * smoothstep(.48, .60, diffuse);
    if (fourBand)
        bands = .20 + .20 * smoothstep(.15, .17, diffuse) +
                .28 * smoothstep(.39, .41, diffuse) + .32 * smoothstep(.71, .73, diffuse);
    float sky = .04 * max(n.z, 0.0);
    float contact = mix(.86, 1.0, smoothstep(0.0, 25.0, worldPosition.z));
    vec3 view = normalize(vec3(.603, .603, .519));
    float rim = pow(1.0 - max(dot(n, view), 0.0), 3.0) * .035;
    vec3 rgb = baseColor.rgb * (bands + sky) * contact + vec3(.55, .65, 1.0) * rim;
    if (max(baseColor.r, max(baseColor.g, baseColor.b)) > 1.0)
        rgb = baseColor.rgb;
    outputColor = vec4(rgb, 1);
}
