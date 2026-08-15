#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec3 theme = mix(vec3(0.62, 0.18, 1.0), colorAccent.rgb, 0.2);
    vec2 fromC = spriteUV(uv) - vec2(0.5);
    float ang = atan(fromC.y, fromC.x) + t * 1.3;
    vec2 flow = p + vec2(cos(ang), sin(ang)) * (0.4 + length(fromC));
    return cannonShell(theme, 6.2, flow, uv, t, mask);
}
