#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 2.2) - mask);
    float halo = max(0.0, dilateRing(uv, 7.0) - mask);
    if (mask < 0.08 && outline < 0.02 && halo < 0.02) {
        return vec4(0.0);
    }

    vec2 flow = p + vec2(-t * 1.35, -t * 1.85);
    float smoke = fluidField(flow, t * 1.55, 1.05);
    float wisp = pow(clamp(smoke, 0.0, 1.0), 1.15) * halo;
    float pulse = 0.45 + 0.55 * sin(t * 6.2 + uv.x * 9.0);
    vec3 live = mix(vec3(0.42, 0.44, 0.52), vec3(0.96, 0.97, 1.0), smoke * pulse);
    vec4 glyph = glyphOnce(live, mask, outline, 1.0);
    vec4 aura = nameSoftAura(live, uv, mask, t);
    vec3 plume = mix(vec3(0.18, 0.18, 0.22), vec3(0.72, 0.74, 0.80), wisp);
    float plumeA = smoothstep(0.10, 0.42, wisp) * (1.0 - mask) * 0.62;
    vec3 col = mix(aura.rgb, plume, plumeA);
    col = mix(col, glyph.rgb, glyph.a);
    return vec4(col, clamp(max(max(glyph.a, aura.a), plumeA), 0.0, 1.0));
}
