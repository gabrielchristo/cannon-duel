void main() {
    vec2 uv = fragTexCoord;
    vec2 aspect = resolution / max(resolution.x, resolution.y);
    vec2 p = spriteUV(uv) * aspect * 4.2;
    FRAG_OUT = shade(uv, p, time, sampleMask(uv));
    if (FRAG_OUT.a < DISCARD_A) {
        discard;
    }
}
