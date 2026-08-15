// Núcleo compartilhado — concatenado com preamble + efeito + pass.inc
// SAMPLE e FRAG_OUT vêm do preamble (GL 330 vs ES 100).

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.05;
        a *= 0.5;
    }
    return v;
}

vec2 fluidWarp(vec2 p, float t) {
    vec2 q = vec2(
        fbm(p + vec2(0.0, t * 0.31)),
        fbm(p + vec2(5.2, t * 0.27))
    );
    vec2 r = vec2(
        fbm(p + 3.7 * q + vec2(1.7, t * 0.22)),
        fbm(p + 3.7 * q + vec2(8.3, t * 0.19))
    );
    return p + 0.42 * r;
}

float fluidField(vec2 p, float t, float speed) {
    vec2 w = fluidWarp(p, t * speed);
    float n1 = fbm(w + vec2(t * 0.18, -t * 0.42));
    float n2 = fbm(w * 1.7 - vec2(t * 0.25, t * 0.15));
    return mix(n1, n2, 0.45);
}

vec2 spriteUV(vec2 uv) {
    return (uv - vec2(0.5)) * max(uvScale, 1.0) + vec2(0.5);
}

float sampleMask(vec2 uv) {
    vec2 suv = spriteUV(uv);
    if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
        return 0.0;
    }
    return SAMPLE(texture0, suv).a;
}

float neighborMax(vec2 uv, float px) {
    vec2 s = vec2(px) / max(resolution, vec2(1.0));
    float m = sampleMask(uv);
    m = max(m, sampleMask(uv + vec2(s.x, 0.0)));
    m = max(m, sampleMask(uv + vec2(-s.x, 0.0)));
    m = max(m, sampleMask(uv + vec2(0.0, s.y)));
    m = max(m, sampleMask(uv + vec2(0.0, -s.y)));
    m = max(m, sampleMask(uv + vec2(s.x, s.y)));
    m = max(m, sampleMask(uv + vec2(-s.x, s.y)));
    m = max(m, sampleMask(uv + vec2(s.x, -s.y)));
    m = max(m, sampleMask(uv + vec2(-s.x, -s.y)));
    return m;
}

float dilateRing(vec2 uv, float px) {
    vec2 s = vec2(px) / max(resolution, vec2(1.0));
    float m = sampleMask(uv);
    for (int i = 0; i < 16; i++) {
        float a = 6.2831853 * float(i) / 16.0;
        m = max(m, sampleMask(uv + vec2(cos(a), sin(a)) * s));
    }
    return m;
}

float erodeRing(vec2 uv, float px) {
    vec2 s = vec2(px) / max(resolution, vec2(1.0));
    float m = sampleMask(uv);
    for (int i = 0; i < 16; i++) {
        float a = 6.2831853 * float(i) / 16.0;
        m = min(m, sampleMask(uv + vec2(cos(a), sin(a)) * s));
    }
    return m;
}

vec3 emberPalette(float x) {
    vec3 c0 = vec3(0.28, 0.02, 0.0);
    vec3 c1 = vec3(0.95, 0.18, 0.02);
    vec3 c2 = vec3(1.0, 0.55, 0.06);
    vec3 c3 = vec3(1.0, 0.92, 0.45);
    float t0 = smoothstep(0.0, 0.35, x);
    float t1 = smoothstep(0.35, 0.7, x);
    float t2 = smoothstep(0.7, 1.0, x);
    return mix(mix(mix(c0, c1, t0), c2, t1), c3, t2);
}

vec3 holographicSheen(float sweep, float t) {
    return 0.5 + 0.5 * cos(vec3(0.0, 2.094, 4.188) + sweep * 6.28318 + t * 1.6);
}

vec3 prideRamp(float x) {
    float u = fract(x);
    vec3 c0 = vec3(0.90, 0.08, 0.12);
    vec3 c1 = vec3(0.98, 0.52, 0.08);
    vec3 c2 = vec3(0.98, 0.86, 0.12);
    vec3 c3 = vec3(0.12, 0.64, 0.28);
    vec3 c4 = vec3(0.16, 0.38, 0.88);
    vec3 c5 = vec3(0.56, 0.14, 0.64);
    float s = u * 6.0;
    if (s < 1.0) return mix(c0, c1, s);
    if (s < 2.0) return mix(c1, c2, s - 1.0);
    if (s < 3.0) return mix(c2, c3, s - 2.0);
    if (s < 4.0) return mix(c3, c4, s - 3.0);
    if (s < 5.0) return mix(c4, c5, s - 4.0);
    return mix(c5, c0, s - 5.0);
}

vec4 glyphOnce(vec3 live, float mask, float outline, float outlineAmt) {
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    vec3 col = mix(vec3(0.0), live, smoothstep(0.22, 0.72, mask));
    float alpha = max(mask, outline * max(outlineAmt, 1.0));
    return vec4(col, clamp(alpha, 0.0, 1.0));
}

vec4 nameSoftAura(vec3 theme, vec2 uv, float mask, float t) {
    float halo = max(0.0, dilateRing(uv, 3.6) - neighborMax(uv, 3.2));
    if (halo < 0.02) {
        return vec4(0.0);
    }
    float a = halo * (0.09 + 0.03 * sin(t * 2.2));
    return vec4(theme, clamp(a, 0.0, 0.14));
}

vec4 nameLetter(vec3 theme, vec2 flow, float sweep, float osc, float holoAmt,
                float t, float mask, vec2 uv) {
    float outline = max(0.0, neighborMax(uv, 3.2) - mask);
    float field = fluidField(flow, t, 0.85);
    float band = pow(abs(sin(sweep * 3.14159)), 8.0);
    vec3 holo = holographicSheen(sweep, t);
    vec3 pair = mix(colorPrimary.rgb, colorAccent.rgb, osc);
    vec3 live = mix(theme, pair, 0.35);
    live = mix(live, holo, holoAmt * 0.35);
    live = mix(live, theme, field * 0.08);
    live = mix(live, vec3(1.0), band * 0.12);
    vec4 glyph = glyphOnce(live, mask, outline, 1.0);
    vec4 aura = nameSoftAura(live, uv, mask, t);
    return vec4(mix(aura.rgb, glyph.rgb, glyph.a), clamp(max(glyph.a, aura.a), 0.0, 1.0));
}

vec4 cannonShell(vec3 theme, float shellPx, vec2 flowUv, vec2 uv, float t, float mask) {
    float outer = max(0.0, dilateRing(uv, shellPx) - mask);
    float rim = max(0.0, dilateRing(uv, 2.2) - mask);
    float edge = mask * (1.0 - erodeRing(uv, 1.6));
    if (outer < 0.02 && edge < 0.02) {
        return vec4(0.0);
    }
    vec2 suv = spriteUV(uv);
    vec2 fromC = suv - vec2(0.5);
    float ang = atan(fromC.y, fromC.x);
    float field = fluidField(flowUv, t, 1.05);
    float sweep = ang / 6.2831853 + t * 0.42;
    float band = pow(0.5 + 0.5 * sin(sweep * 12.566 + t * 2.4), 3.0);
    vec3 holo = holographicSheen(sweep * 2.0, t);
    vec3 col = mix(theme, holo, 0.22);
    col = mix(col, vec3(1.0), band * 0.35 + field * 0.08);
    float pulse = 0.7 + 0.15 * sin(t * 5.0 + ang * 3.0);
    float alpha = max(edge * 0.45, max(rim * 0.5 * pulse, outer * (0.28 + 0.22 * field)));
    return vec4(col, clamp(alpha, 0.0, 1.0));
}
