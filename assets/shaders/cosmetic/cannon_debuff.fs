#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(0.08, 0.95, 0.18), vec3(0.35, 1.0, 0.22), 0.4);
    vec2 flow = p + vec2(0.0, t * 0.7);
    return cannonShell(theme, 7.0, flow, uv, t, mask);
}
