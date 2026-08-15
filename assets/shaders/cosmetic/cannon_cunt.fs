#define DISCARD_A 0.04

vec4 shade(vec2 uv, vec2 p, float t, float mask) {
    vec2 suv = spriteUV(uv);
    vec2 fromC = suv - vec2(0.5);
    float ang = atan(fromC.y, fromC.x);
    vec3 theme = prideRamp(ang / 6.28318 + t * 0.18);
    vec2 flow = p + vec2(cos(ang + t * 0.9), sin(ang + t * 0.9)) * 0.35;
    return cannonShell(theme, 13.5, flow, uv, t, mask);
}
