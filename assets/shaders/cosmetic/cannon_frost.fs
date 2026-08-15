#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(0.25, 0.78, 1.0), colorAccent.rgb, 0.2);
    vec2 suv = spriteUV(uv);
    vec2 flow = p + vec2(t * 0.85, sin(suv.y * 8.0 + t) * 0.2);
    return cannonShell(theme, 8.0, flow, uv, t, mask);
}
