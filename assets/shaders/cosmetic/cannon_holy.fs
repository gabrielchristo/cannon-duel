#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(1.0, 0.82, 0.15), colorAccent.rgb, 0.2);
    vec2 fromC = spriteUV(uv) - vec2(0.5);
    vec2 flow = p + fromC * (0.3 + 0.25 * sin(t * 2.0));
    return cannonShell(theme, 7.0, flow, uv, t, mask);
}
