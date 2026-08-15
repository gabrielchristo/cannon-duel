#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float wave = sin(uv.x * 14.0 - t * 3.4);
    vec3 theme = mix(vec3(0.00, 0.42, 1.0), vec3(0.10, 0.92, 1.0), 0.45 + 0.45 * wave);
    vec2 flow = p + vec2(t * 1.05, wave * 0.4);
    float sweep = uv.x * 4.6 - t * 2.0;
    float osc = 0.5 + 0.5 * sin(t * 2.9 + uv.x * 10.0);
    return nameLetter(theme, flow, sweep, osc, 0.28, t, mask, uv);
}
