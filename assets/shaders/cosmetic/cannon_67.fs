#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(1.0, 0.82, 0.18), colorAccent.rgb, 0.25);
    vec2 suv = spriteUV(uv);
    vec2 flow = p + vec2(sin(suv.y * 9.0 + t * 6.0) * 0.18, sin(t * 8.0) * 0.12);
    return cannonShell(theme, 5.8, flow, uv, t, mask);
}
