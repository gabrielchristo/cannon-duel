#define DISCARD_A 0.05

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    float outline = max(0.0, neighborMax(uv, 3.4) - mask);
    if (mask < 0.08 && outline < 0.02) {
        return vec4(0.0);
    }
    float petal = 0.5 + 0.5 * sin((uv.x - uv.y) * 10.0 + t * 2.4);
    vec3 live = mix(vec3(1.0, 0.12, 0.48), vec3(1.0, 0.42, 0.72), petal);
    live = mix(live, colorAccent.rgb, fluidField(p, t, 0.35) * 0.08);
    return glyphOnce(live, mask, outline, 1.0);
}
