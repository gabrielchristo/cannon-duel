#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    float stripe = uv.x * 1.65 + t * 3.6;
    vec3 live = prideRamp(stripe + p.x * 0.02);
    live = mix(live, prideRamp(stripe + 0.04), 0.18);
    return glyphOnce(live, mask, outline, 1.0);
}
