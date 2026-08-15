#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(1.0, 0.32, 0.04), colorAccent.rgb, 0.2);
    vec2 suv = spriteUV(uv);
    vec2 flow = p + vec2(sin(suv.x * 10.0 + t * 3.0) * 0.2, -t * 1.1);
    return cannonShell(theme, 7.0, flow, uv, t, mask);
}
