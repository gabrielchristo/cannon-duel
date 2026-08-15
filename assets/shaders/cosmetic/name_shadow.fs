#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    vec2 flow = p + vec2(-t * 0.55, -t * 0.85);
    float smoke = fluidField(flow, t, 0.62);
    vec3 live = mix(vec3(0.78, 0.80, 0.88), vec3(0.96, 0.97, 1.0), smoke);
    return glyphOnce(live, mask, outline, 1.0);
}
