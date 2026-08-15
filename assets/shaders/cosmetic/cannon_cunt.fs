#define DISCARD_A 0.03

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outer = max(0.0, dilateRing(uv, 12.0) - mask);
    float rim = max(0.0, dilateRing(uv, 2.4) - mask);
    float edge = mask * (1.0 - erodeRing(uv, 1.8));
    if (outer < 0.02 && edge < 0.02) {
        return vec4(0.0);
    }
    vec2 suv = spriteUV(uv);
    vec2 fromC = suv - vec2(0.5);
    float ang = atan(fromC.y, fromC.x);
    vec2 flow = p + vec2(cos(ang + t * 1.1), sin(ang + t * 1.1)) * 0.55;
    float field = fluidField(flow, t, 1.15);
    float liquid = pow(clamp(field, 0.0, 1.0), 0.72) * outer;
    vec3 col = prideRamp(ang / 6.2831853 + t * 0.55 + field * 0.35);
    col = mix(col, prideRamp(ang / 6.2831853 + t * 0.55 + 0.12), 0.28);
    col = mix(col, vec3(1.0), liquid * 0.18);
    float alpha = max(edge * 0.42, max(rim * 0.5, liquid * 0.55));
    return vec4(col, clamp(alpha, 0.0, 1.0));
}
