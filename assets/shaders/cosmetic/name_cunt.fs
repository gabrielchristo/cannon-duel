#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.2) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    float stripe = uv.x * 1.65 + t * 3.6;
    vec3 live = prideRamp(stripe + p.x * 0.02);
    live = mix(live, prideRamp(stripe + 0.04), 0.18);
    vec4 glyph = glyphOnce(live, mask, outline, 1.0);
    vec4 aura = nameSoftAura(live, uv, mask, t);
    return vec4(mix(aura.rgb, glyph.rgb, glyph.a), clamp(max(glyph.a, aura.a), 0.0, 1.0));
}
