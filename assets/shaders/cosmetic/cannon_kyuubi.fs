#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(1.0, 0.16, 0.02), colorAccent.rgb, 0.18);
    vec2 suv = spriteUV(uv);
    vec2 flow = p + vec2(sin(suv.x * 9.0 + t * 4.2) * 0.28, -t * 1.45);
    return cannonShell(theme, 6.4, flow, uv, t, mask);
}
