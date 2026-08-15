#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float petal = 0.5 + 0.5 * sin((uv.x - uv.y) * 10.0 + t * 2.4);
    vec3 theme = mix(vec3(1.0, 0.12, 0.48), vec3(1.0, 0.42, 0.72), petal);
    vec2 flow = p + vec2(t * 0.5, t * 0.85);
    float sweep = (uv.x + uv.y) * 3.6 - t * 1.6;
    return nameLetter(theme, flow, sweep, petal, 0.22, t, mask, uv);
}
