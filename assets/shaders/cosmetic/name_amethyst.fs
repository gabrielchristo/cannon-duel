#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    vec2 c = uv - vec2(0.5);
    float pulse = 0.5 + 0.5 * sin(t * 3.1 + length(c) * 12.0);
    float facet = floor(fluidField(p + c * 1.4, t, 0.9) * 5.0) / 5.0;
    vec3 live = mix(vec3(0.72, 0.08, 1.0), vec3(1.0, 0.35, 1.0), facet * 0.55 + pulse * 0.45);
    return glyphOnce(live, mask, outline, 1.0);
}
