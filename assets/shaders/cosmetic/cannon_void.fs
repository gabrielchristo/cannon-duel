#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(0.55, 0.12, 1.0), colorAccent.rgb, 0.2);
    vec2 fromC = spriteUV(uv) - vec2(0.5);
    float radial = length(fromC);
    vec2 flow = p - fromC * (0.8 + t * 0.15) * (1.2 - radial);
    return cannonShell(theme, 8.0, flow, uv, t, mask);
}
