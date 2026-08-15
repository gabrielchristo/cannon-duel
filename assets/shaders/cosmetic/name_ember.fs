#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    float halo = max(0.0, dilateRing(uv, 5.5) - mask);
    if (mask < 0.08 && outline < 0.02 && halo < 0.02) {
        return vec4(0.0);
    }

    vec2 advect = vec2(p.x * 1.8, p.y * 3.0 - t * 3.2);
    float flame = pow(clamp(fbm(advect + vec2(0.0, -t * 1.4)), 0.0, 1.0), 1.15);
    float lick = flame * halo * (0.75 + 0.25 * sin(t * 7.0 + uv.x * 16.0));
    float heat = 0.5 + 0.5 * sin(t * 5.4 + uv.x * 10.0);
    vec3 live = mix(vec3(1.0, 0.28, 0.04), vec3(1.0, 0.82, 0.12), heat);
    vec4 glyph = glyphOnce(live, mask, outline, 1.0);
    vec3 fire = emberPalette(lick);
    float fireA = smoothstep(0.14, 0.46, lick) * (1.0 - mask);
    return vec4(mix(fire, glyph.rgb, glyph.a), clamp(max(glyph.a, fireA), 0.0, 1.0));
}
