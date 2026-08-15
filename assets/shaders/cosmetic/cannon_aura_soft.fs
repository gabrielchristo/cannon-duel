#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(0.35, 0.75, 1.0), colorAccent.rgb, 0.25);
    vec2 fromC = spriteUV(uv) - vec2(0.5);
    vec2 flow = p + fromC * 0.25 + vec2(0.0, -t * 0.2);
    return cannonShell(theme, 13.0, flow, uv, t, mask);
}
