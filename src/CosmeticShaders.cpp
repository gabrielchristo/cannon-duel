#include "CosmeticShaders.h"
#include "AssetPath.h"
#include "Platform.h"

#include <algorithm>
#include <cmath>
#include <string>

CosmeticShaders gCosmeticShaders;

namespace {

std::string LoadAssetText(const char* relative) {
    const std::string path = AssetPath(relative);
    char* text = LoadFileText(path.c_str());
    if (!text) return {};
    std::string out(text);
    UnloadFileText(text);
    return out;
}

const char* Preamble() {
#if CANNON_DUEL_WEB_BUILD || CANNON_DUEL_ANDROID_BUILD
    return R"(#version 100
precision mediump float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform float time;
uniform vec4 colorPrimary;
uniform vec4 colorAccent;
uniform vec2 resolution;
uniform float uvScale;
#define SAMPLE texture2D
#define FRAG_OUT gl_FragColor
)";
#else
    return R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float time;
uniform vec4 colorPrimary;
uniform vec4 colorAccent;
uniform vec2 resolution;
uniform float uvScale;
#define SAMPLE texture
#define FRAG_OUT finalColor
)";
#endif
}

} // namespace

void CosmeticShaders::UnloadProgram(Program* program) {
    if (program->shader.id != 0) UnloadShader(program->shader);
    *program = {};
}

bool CosmeticShaders::CompileProgram(Program* out, const std::string& common, const std::string& pass,
                                     const char* effectRelPath) {
    const std::string effect = LoadAssetText(effectRelPath);
    if (effect.empty()) return false;

    const std::string source = std::string(Preamble()) + "\n" + common + "\n" + effect + "\n" + pass;
    out->shader = LoadShaderFromMemory(nullptr, source.c_str());
    if (out->shader.id == 0) {
        TraceLog(LOG_WARNING, "Cosmetic shader failed: %s", effectRelPath);
        return false;
    }

    out->locTime = GetShaderLocation(out->shader, "time");
    out->locPrimary = GetShaderLocation(out->shader, "colorPrimary");
    out->locAccent = GetShaderLocation(out->shader, "colorAccent");
    out->locResolution = GetShaderLocation(out->shader, "resolution");
    out->locUvScale = GetShaderLocation(out->shader, "uvScale");
    // Uniforms não usados no .fs são otimizados pelo driver (loc = -1).
    // O shader continua válido — ApplyUniforms ignora locations negativos.
    out->ready = true;
    return true;
}

void CosmeticShaders::Init() {
    if (ready_) return;

    const std::string common = LoadAssetText("shaders/cosmetic/common.inc");
    const std::string pass = LoadAssetText("shaders/cosmetic/pass.inc");
    if (common.empty() || pass.empty()) return;

    static constexpr const char* kNameFiles[kNameProgramCount] = {
        "shaders/cosmetic/name_ember.fs",
        "shaders/cosmetic/name_shadow.fs",
        "shaders/cosmetic/name_amethyst.fs",
        "shaders/cosmetic/name_cunt.fs",
        "shaders/cosmetic/name_ocean.fs",
        "shaders/cosmetic/name_sakura.fs",
    };
    static constexpr const char* kCannonFiles[kCannonProgramCount] = {
        "shaders/cosmetic/cannon_aura_soft.fs",
        "shaders/cosmetic/cannon_aura_fire.fs",
        "shaders/cosmetic/cannon_holy.fs",
        "shaders/cosmetic/cannon_debuff.fs",
        "shaders/cosmetic/cannon_arcane.fs",
        "shaders/cosmetic/cannon_inferno.fs",
        "shaders/cosmetic/cannon_frost.fs",
        "shaders/cosmetic/cannon_void.fs",
        "shaders/cosmetic/cannon_cunt.fs",
        "shaders/cosmetic/cannon_67.fs",
        "shaders/cosmetic/cannon_kyuubi.fs",
    };

    bool any = false;
    for (int i = 0; i < kNameProgramCount; ++i) {
        any = CompileProgram(&namePrograms_[static_cast<size_t>(i)], common, pass, kNameFiles[i]) || any;
    }
    for (int i = 0; i < kCannonProgramCount; ++i) {
        any = CompileProgram(&cannonPrograms_[static_cast<size_t>(i)], common, pass, kCannonFiles[i]) || any;
    }
    ready_ = any;
}

void CosmeticShaders::Shutdown() {
    if (textMaskTex_.id != 0) UnloadTexture(textMaskTex_);
    textMaskTex_ = {};
    for (Program& program : namePrograms_) UnloadProgram(&program);
    for (Program& program : cannonPrograms_) UnloadProgram(&program);
    ready_ = false;
    textMaskAllocW_ = 0;
    textMaskAllocH_ = 0;
    textMaskCache_.clear();
}

bool CosmeticShaders::UsesFluidShader(CannonEffectStyle style) {
    return style != CannonEffectStyle::None;
}

bool CosmeticShaders::UsesLiquidShader(NameEffectStyle style) {
    return style != NameEffectStyle::Plain;
}

int CosmeticShaders::NameEffectShaderIndex(NameEffectStyle style) {
    switch (style) {
        case NameEffectStyle::Flame: return 0;
        case NameEffectStyle::DarkSmoke: return 1;
        case NameEffectStyle::PurpleGlow: return 2;
        case NameEffectStyle::Cunt: return 3;
        case NameEffectStyle::OceanWave: return 4;
        case NameEffectStyle::Sakura: return 5;
        default: return 0;
    }
}

int CosmeticShaders::CannonEffectShaderIndex(CannonEffectStyle style) {
    switch (style) {
        case CannonEffectStyle::AuraSoft: return 0;
        case CannonEffectStyle::AuraFire: return 1;
        case CannonEffectStyle::ImbueHoly: return 2;
        case CannonEffectStyle::DebuffGlow: return 3;
        case CannonEffectStyle::ArcaneSpark: return 4;
        case CannonEffectStyle::LiquidInferno: return 5;
        case CannonEffectStyle::LiquidFrost: return 6;
        case CannonEffectStyle::LiquidVoid: return 7;
        case CannonEffectStyle::AuraCunt: return 8;
        case CannonEffectStyle::SixSeven: return 9;
        case CannonEffectStyle::Kyuubi: return 10;
        default: return 0;
    }
}

void CosmeticShaders::ApplyUniforms(const Program& program, Color primary, Color accent,
                                    float w, float h, float uvScale, float timeScale) {
    const float t = static_cast<float>(GetTime()) * timeScale;
    if (program.locTime >= 0) {
        SetShaderValue(program.shader, program.locTime, &t, SHADER_UNIFORM_FLOAT);
    }

    const float primaryNorm[4] = {
        primary.r / 255.0f, primary.g / 255.0f, primary.b / 255.0f, primary.a / 255.0f
    };
    const float accentNorm[4] = {
        accent.r / 255.0f, accent.g / 255.0f, accent.b / 255.0f, accent.a / 255.0f
    };
    if (program.locPrimary >= 0) {
        SetShaderValue(program.shader, program.locPrimary, primaryNorm, SHADER_UNIFORM_VEC4);
    }
    if (program.locAccent >= 0) {
        SetShaderValue(program.shader, program.locAccent, accentNorm, SHADER_UNIFORM_VEC4);
    }

    const float res[2] = { w, h };
    if (program.locResolution >= 0) {
        SetShaderValue(program.shader, program.locResolution, res, SHADER_UNIFORM_VEC2);
    }
    if (program.locUvScale >= 0) {
        SetShaderValue(program.shader, program.locUvScale, &uvScale, SHADER_UNIFORM_FLOAT);
    }
}

bool CosmeticShaders::EnsureTextMask(const char* text, int fontSize, int rw, int rh, int padX, int padY) {
    const std::string cacheKey = std::string(text) + "|" + std::to_string(fontSize)
        + "|" + std::to_string(rw) + "|" + std::to_string(rh)
        + "|" + std::to_string(padX) + "|" + std::to_string(padY);

    if (cacheKey == textMaskCache_ && textMaskTex_.id != 0
        && textMaskAllocW_ == rw && textMaskAllocH_ == rh) {
        return true;
    }

    textMaskAllocW_ = rw;
    textMaskAllocH_ = rh;

    Image img = GenImageColor(rw, rh, BLANK);
    ImageDrawText(&img, text, padX, padY, fontSize, WHITE);
    if (textMaskTex_.id != 0) UnloadTexture(textMaskTex_);
    textMaskTex_ = LoadTextureFromImage(img);
    UnloadImage(img);

    if (textMaskTex_.id == 0) return false;

    SetTextureWrap(textMaskTex_, TEXTURE_WRAP_CLAMP);
    SetTextureFilter(textMaskTex_, TEXTURE_FILTER_BILINEAR);
    textMaskCache_ = cacheKey;
    return true;
}

bool CosmeticShaders::DrawNameEffect(const char* text, int x, int y, int fontSize,
                                     NameEffectStyle style, Color primary, Color accent) {
    if (!ready_ || !UsesLiquidShader(style) || !text || !text[0]) return false;

    const int tw = MeasureText(text, fontSize);
    if (tw <= 0) return false;

    const bool extraPad = (style == NameEffectStyle::Flame || style == NameEffectStyle::DarkSmoke);
    const int padX = extraPad ? 12 : 8;
    const int padTop = extraPad ? 12 : 8;
    const int padBottom = extraPad ? 10 : 8;
    const int rw = tw + padX * 2;
    const int rh = fontSize + padTop + padBottom;

    if (!EnsureTextMask(text, fontSize, rw, rh, padX, padTop)) return false;

    const int index = NameEffectShaderIndex(style);
    const Program& program = namePrograms_[static_cast<size_t>(index)];
    if (!program.ready) return false;

    const float timeScale = (style == NameEffectStyle::DarkSmoke) ? 1.45f : 0.62f;
    ApplyUniforms(program, primary, accent, static_cast<float>(rw), static_cast<float>(rh), 1.0f, timeScale);

    BeginShaderMode(program.shader);
    DrawTexturePro(textMaskTex_,
                   { 0, 0, static_cast<float>(textMaskTex_.width), static_cast<float>(textMaskTex_.height) },
                   { static_cast<float>(x - padX), static_cast<float>(y - padTop),
                     static_cast<float>(rw), static_cast<float>(rh) },
                   { 0, 0 }, 0.0f, WHITE);
    EndShaderMode();

    return true;
}

namespace {

Color LerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        255
    };
}

float NameMotionSpeed(NameEffectStyle style) {
    switch (style) {
        case NameEffectStyle::DarkSmoke: return 2.6f;
        case NameEffectStyle::Flame: return 2.6f;
        case NameEffectStyle::PurpleGlow: return 2.2f;
        case NameEffectStyle::OceanWave: return 2.5f;
        case NameEffectStyle::Sakura: return 2.8f;
        case NameEffectStyle::Cunt: return 3.2f;
        default: return 1.8f;
    }
}

Color ThemeAccent(Color primary, Color accent) {
    const int lum = accent.r + accent.g + accent.b;
    if (lum < 90) {
        return Color{ 190, 195, 210, 255 };
    }
    return Color{
        static_cast<unsigned char>(std::min(255, (primary.r + accent.r) / 2 + 40)),
        static_cast<unsigned char>(std::min(255, (primary.g + accent.g) / 2 + 40)),
        static_cast<unsigned char>(std::min(255, (primary.b + accent.b) / 2 + 40)),
        255
    };
}

} // namespace

void CosmeticShaders::DrawNameOrnaments(const char* text, int x, int y, int fontSize,
                                        NameEffectStyle style, Color primary, Color accent) {
    if (!text || !text[0] || style == NameEffectStyle::Plain) return;

    const int tw = MeasureText(text, fontSize);
    if (tw <= 0) return;

    const float t = static_cast<float>(GetTime());
    const Color theme = ThemeAccent(primary, accent);
    const float midX = static_cast<float>(x) + static_cast<float>(tw) * 0.5f;
    const float midY = static_cast<float>(y) + static_cast<float>(fontSize) * 0.45f;

    switch (style) {
        case NameEffectStyle::Flame:
            DrawEmberFire(x, y, tw, fontSize);
            return;
        case NameEffectStyle::DarkSmoke:
            DrawShadowSmoke(x, y, tw, fontSize);
            return;
        case NameEffectStyle::PurpleGlow:
            for (int i = 0; i < 8; ++i) {
                const float ang = t * 1.3f + static_cast<float>(i) * (PI * 0.25f);
                const float px = midX + std::cos(ang) * (static_cast<float>(tw) * 0.52f);
                const float py = midY + std::sin(ang) * (static_cast<float>(fontSize) * 0.7f);
                DrawCircleV({ px, py }, 1.6f, Fade(theme, 0.45f));
                DrawCircleV({ px, py }, 0.7f, Fade(WHITE, 0.4f));
            }
            return;
        case NameEffectStyle::OceanWave:
            for (int i = 0; i < 10; ++i) {
                const float u = (static_cast<float>(i) + std::fmod(t * 1.1f, 1.0f)) / 10.0f;
                const float px = static_cast<float>(x) + u * static_cast<float>(tw);
                const float py = midY + std::sin(u * 6.28318f - t * 3.2f) * (fontSize * 0.38f);
                DrawCircleV({ px, py }, 1.4f, Fade(theme, 0.4f));
            }
            return;
        case NameEffectStyle::Sakura:
            for (int i = 0; i < 7; ++i) {
                const float seed = static_cast<float>(i) * 1.9f;
                const float life = std::fmod(t * 0.28f + seed * 0.15f, 1.0f);
                const float px = static_cast<float>(x) + std::fmod(seed * 37.0f, static_cast<float>(tw))
                    + life * 10.0f;
                const float py = static_cast<float>(y) - 4.0f + life * (fontSize + 14.0f);
                DrawEllipse(static_cast<int>(px), static_cast<int>(py), 3, 2,
                            Fade(theme, (1.0f - life) * 0.5f));
            }
            return;
        case NameEffectStyle::Cunt: {
            static const Color kPride[] = {
                { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
                { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
            };
            for (int i = 0; i < 6; ++i) {
                const float u = std::fmod((static_cast<float>(i) + 0.5f) / 6.0f + t * 0.55f, 1.0f);
                const float px = static_cast<float>(x) + u * static_cast<float>(tw);
                const float py = static_cast<float>(y) - 3.0f + std::sin(t * 5.5f + u * 8.0f) * 2.0f;
                DrawCircleV({ px, py }, 1.5f, Fade(kPride[i], 0.55f));
            }
            return;
        }
        default:
            return;
    }
}

void CosmeticShaders::DrawStyledNameCpu(const char* text, int x, int y, int fontSize,
                                        NameEffectStyle style, Color primary, Color accent) {
    if (!text || !text[0]) return;

    const float t = static_cast<float>(GetTime());
    const float osc = 0.5f + 0.5f * std::sin(t * NameMotionSpeed(style));
    Color textColor = LerpColor(primary, accent, osc);
    switch (style) {
        case NameEffectStyle::Flame:
            textColor = LerpColor(Color{ 255, 70, 10, 255 }, Color{ 255, 210, 30, 255 }, osc);
            break;
        case NameEffectStyle::DarkSmoke:
            textColor = LerpColor(Color{ 210, 214, 230, 255 }, Color{ 245, 246, 255, 255 }, osc);
            break;
        case NameEffectStyle::PurpleGlow:
            textColor = LerpColor(Color{ 185, 20, 255, 255 }, Color{ 255, 90, 255, 255 }, osc);
            break;
        case NameEffectStyle::OceanWave:
            textColor = LerpColor(Color{ 0, 110, 255, 255 }, Color{ 20, 230, 255, 255 }, osc);
            break;
        case NameEffectStyle::Sakura:
            textColor = LerpColor(Color{ 255, 30, 120, 255 }, Color{ 255, 110, 180, 255 }, osc);
            break;
        case NameEffectStyle::Cunt: {
            static const Color kPride[] = {
                { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
                { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
            };
            const int totalW = MeasureText(text, fontSize);
            int cx = x;
            for (const char* p = text; *p; ++p) {
                const char buf[2] = { *p, '\0' };
                const int cw = MeasureText(buf, fontSize);
                const float u = totalW > 0
                    ? (static_cast<float>(cx - x) + static_cast<float>(cw) * 0.5f)
                        / static_cast<float>(totalW)
                    : 0.0f;
                float s = std::fmod(u * 1.65f + t * 3.6f, 6.0f);
                if (s < 0.0f) s += 6.0f;
                const int i0 = static_cast<int>(s) % 6;
                const Color col = LerpColor(kPride[i0], kPride[(i0 + 1) % 6], s - std::floor(s));
                DrawText(buf, cx - 1, y, fontSize, BLACK);
                DrawText(buf, cx + 1, y, fontSize, BLACK);
                DrawText(buf, cx, y - 1, fontSize, BLACK);
                DrawText(buf, cx, y + 1, fontSize, BLACK);
                DrawText(buf, cx, y, fontSize, col);
                cx += cw;
            }
            return;
        }
        case NameEffectStyle::Plain:
            textColor = primary.r + primary.g + primary.b < 40 ? WHITE : primary;
            break;
        default:
            break;
    }

    DrawText(text, x - 1, y, fontSize, BLACK);
    DrawText(text, x + 1, y, fontSize, BLACK);
    DrawText(text, x, y - 1, fontSize, BLACK);
    DrawText(text, x, y + 1, fontSize, BLACK);
    DrawText(text, x, y, fontSize, textColor);
}

void CosmeticShaders::DrawStyledName(const char* text, int x, int y, int fontSize,
                                     NameEffectStyle style, Color primary, Color accent) {
    if (!text || !text[0]) return;
    DrawNameOrnaments(text, x, y, fontSize, style, primary, accent);
    bool shaderDrew = false;
    if (ready_ && UsesLiquidShader(style)) {
        shaderDrew = DrawNameEffect(text, x, y, fontSize, style, primary, accent);
    }
    if (!shaderDrew) {
        DrawStyledNameCpu(text, x, y, fontSize, style, primary, accent);
    }
}

void CosmeticShaders::DrawShadowSmoke(int x, int y, int width, int fontSize) {
    const float t = static_cast<float>(GetTime());
    const float baseY = static_cast<float>(y + fontSize) - 1.0f;
    const float span = std::max(8.0f, static_cast<float>(width));
    for (int i = 0; i < 11; ++i) {
        const float seed = static_cast<float>(i) * 1.83f;
        const float life = std::fmod(t * 0.42f + seed * 0.23f, 1.0f);
        const float slot = std::fmod(seed * 0.41f + t * 0.08f, 1.0f);
        const float px = static_cast<float>(x) + slot * span
            - life * (18.0f + std::sin(seed) * 6.0f)
            + std::sin(t * 2.8f + seed) * 3.0f;
        const float rise = life * (static_cast<float>(fontSize) + 22.0f);
        const float py = baseY - rise;
        const float fade = (1.0f - life) * (0.55f + 0.25f * std::sin(seed + t));
        const float rx = 2.4f + life * 5.5f;
        const float ry = 1.8f + life * 4.2f;
        DrawEllipse(static_cast<int>(px), static_cast<int>(py),
                    static_cast<int>(rx), static_cast<int>(ry),
                    Fade(Color{ 28, 28, 34, 255 }, fade * 0.55f));
        DrawEllipse(static_cast<int>(px - 1.5f), static_cast<int>(py - 2.0f),
                    static_cast<int>(rx * 0.55f), static_cast<int>(ry * 0.5f),
                    Fade(Color{ 120, 124, 136, 255 }, fade * 0.4f));
    }
}

void CosmeticShaders::DrawEmberFire(int x, int y, int width, int fontSize) {
    const float t = static_cast<float>(GetTime());
    const float baseY = static_cast<float>(y + fontSize) - 2.0f;
    const int tongues = 9;
    for (int i = 0; i < tongues; ++i) {
        const float seed = static_cast<float>(i) * 2.17f;
        const float life = std::fmod(t * 0.35f + seed * 0.27f, 1.0f);
        const float slot = std::fmod(seed * 0.37f + t * 0.03f, 1.0f);
        const float px = static_cast<float>(x) + 6.0f + slot * std::max(0.0f, static_cast<float>(width) - 12.0f)
            + std::sin(t * 2.2f + seed) * 2.0f;
        const float h = (fontSize * 0.45f + 6.0f) * (0.45f + 0.55f * std::sin(seed + t * 1.2f)) * (1.1f - life * 0.4f);
        const float w = 2.0f + (1.0f - life) * 2.4f;
        const float tipY = baseY - h - life * 8.0f;
        DrawEllipse(static_cast<int>(px), static_cast<int>(baseY - h * 0.45f), static_cast<int>(w),
                    static_cast<int>(h * 0.55f), Fade(Color{ 190, 24, 4, 255 }, 0.42f * (1.0f - life * 0.4f)));
        DrawEllipse(static_cast<int>(px), static_cast<int>(baseY - h * 0.7f), static_cast<int>(w * 0.6f),
                    static_cast<int>(h * 0.4f), Fade(Color{ 255, 110, 18, 255 }, 0.55f));
        DrawEllipse(static_cast<int>(px), static_cast<int>(tipY), static_cast<int>(w * 0.32f),
                    static_cast<int>(h * 0.28f), Fade(Color{ 255, 230, 90, 255 }, 0.65f * (1.0f - life)));
    }
}

namespace {

Color CannonThemeColor(CannonEffectStyle style, Color accent) {
    switch (style) {
        case CannonEffectStyle::AuraSoft: return Color{ 70, 170, 255, 255 };
        case CannonEffectStyle::AuraFire: return Color{ 255, 80, 16, 255 };
        case CannonEffectStyle::ImbueHoly: return Color{ 255, 210, 40, 255 };
        case CannonEffectStyle::DebuffGlow: return Color{ 30, 230, 70, 255 };
        case CannonEffectStyle::ArcaneSpark: return Color{ 170, 70, 255, 255 };
        case CannonEffectStyle::LiquidInferno: return Color{ 255, 50, 8, 255 };
        case CannonEffectStyle::LiquidFrost: return Color{ 50, 190, 255, 255 };
        case CannonEffectStyle::LiquidVoid: return Color{ 150, 40, 255, 255 };
        case CannonEffectStyle::SixSeven: return Color{ 255, 214, 70, 255 };
        case CannonEffectStyle::Kyuubi: return Color{ 255, 48, 8, 255 };
        case CannonEffectStyle::AuraCunt: {
            static const Color kPride[] = {
                { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
                { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
            };
            const float t = static_cast<float>(GetTime()) * 3.2f;
            const int i = static_cast<int>(std::floor(std::fmod(t, 6.0f)));
            return kPride[(i % 6 + 6) % 6];
        }
        default: return accent;
    }
}

} // namespace

void CosmeticShaders::DrawCannonOutline(const Texture2D& sprite, Rectangle src, Rectangle dst, Vector2 origin,
                                        CannonEffectStyle style, Color accent) {
    if (sprite.id == 0 || style == CannonEffectStyle::None) return;
    const Color col = Fade(CannonThemeColor(style, accent), 0.26f);
    const float px = 0.7f;
    static constexpr float kOx[] = { 1.0f, -1.0f, 0.0f, 0.0f };
    static constexpr float kOy[] = { 0.0f, 0.0f, 1.0f, -1.0f };
    for (int i = 0; i < 4; ++i) {
        const Rectangle ring = { dst.x + kOx[i] * px, dst.y + kOy[i] * px, dst.width, dst.height };
        DrawTexturePro(sprite, src, ring, origin, 0.0f, col);
    }
}

void CosmeticShaders::DrawCannonEnergyField(Vector2 center, float radius, Color primary, Color accent,
                                           CannonEffectStyle style) {
    if (style != CannonEffectStyle::AuraCunt) {
        (void)primary;
        (void)accent;
        return;
    }
    static const Color kPride[] = {
        { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
        { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
    };
    const float t = static_cast<float>(GetTime());
    for (int i = 0; i < 18; ++i) {
        const float seed = static_cast<float>(i) * 0.41f;
        const float life = std::fmod(t * 0.55f + seed, 1.0f);
        const float ang = seed * 6.28318f + t * 0.85f + std::sin(t * 1.7f + seed) * 0.4f;
        const float rad = radius * (1.05f + 0.55f * std::sin(life * PI + seed));
        const float px = center.x + std::cos(ang) * rad;
        const float py = center.y + std::sin(ang) * rad * 0.78f;
        const float w = 3.6f + 2.8f * (1.0f - life);
        const float h = 2.2f + 1.6f * std::sin(life * PI);
        DrawEllipse(static_cast<int>(px), static_cast<int>(py), static_cast<int>(w),
                    static_cast<int>(h), Fade(kPride[i % 6], 0.28f + 0.22f * std::sin(life * PI)));
    }
}

void CosmeticShaders::DrawCannonCoating(const Texture2D& sprite, Rectangle src, Rectangle dst, Vector2 origin,
                                        CannonEffectStyle style, Color primary, Color accent) {
    if (!ready_ || !UsesFluidShader(style) || sprite.id == 0) return;

    const int index = CannonEffectShaderIndex(style);
    const Program& program = cannonPrograms_[static_cast<size_t>(index)];
    if (!program.ready) return;

    const float grow = 1.16f;
    const Rectangle energyDst = { dst.x, dst.y, dst.width * grow, dst.height * grow };
    const Vector2 energyOrigin = { origin.x * grow, origin.y * grow };
    ApplyUniforms(program, primary, accent,
                  std::fabs(energyDst.width), std::fabs(energyDst.height), grow, 1.0f);

    SetTextureWrap(sprite, TEXTURE_WRAP_CLAMP);
    BeginShaderMode(program.shader);
    DrawTexturePro(sprite, src, energyDst, energyOrigin, 0.0f, WHITE);
    EndShaderMode();
}
