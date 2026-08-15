#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    float wave = sin(uv.x * 14.0 - t * 3.4 + uv.y * 3.0);
    float crest = pow(abs(sin(uv.x * 8.0 - t * 2.6)), 6.0);
    vec3 live = mix(vec3(0.00, 0.42, 1.0), vec3(0.10, 0.92, 1.0), 0.45 + 0.45 * wave);
    live = mix(live, vec3(0.85, 1.0, 1.0), crest * 0.35);
    live = mix(live, colorAccent.rgb, fluidField(p, t, 0.4) * 0.08);
    return glyphOnce(live, mask, outline, 1.0);
}
