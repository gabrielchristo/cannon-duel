#pragma once
#include <raylib.h>
#include <array>
#include <string>
#include "ShopCatalog.h"

class CosmeticShaders {
public:
    void Init();
    void Shutdown();
    bool IsReady() const { return ready_; }

    bool DrawNameEffect(const char* text, int x, int y, int fontSize,
                        NameEffectStyle style, Color primary, Color accent);

    void DrawStyledName(const char* text, int x, int y, int fontSize,
                        NameEffectStyle style, Color primary, Color accent);

    void DrawCannonCoating(const Texture2D& sprite, Rectangle src, Rectangle dst, Vector2 origin,
                           CannonEffectStyle style, Color primary, Color accent);

    void DrawCannonOutline(const Texture2D& sprite, Rectangle src, Rectangle dst, Vector2 origin,
                           CannonEffectStyle style, Color accent);

    void DrawCannonEnergyField(Vector2 center, float radius, Color primary, Color accent,
                               CannonEffectStyle style = CannonEffectStyle::None);
    void DrawEmberFire(int x, int y, int width, int fontSize);
    void DrawShadowSmoke(int x, int y, int width, int fontSize);

    static int NameEffectShaderIndex(NameEffectStyle style);
    static int CannonEffectShaderIndex(CannonEffectStyle style);
    static bool UsesFluidShader(CannonEffectStyle style);
    static bool UsesLiquidShader(NameEffectStyle style);

    // Compat: mesmos índices usados internamente.
    static int NameEffectShaderMode(NameEffectStyle style) { return NameEffectShaderIndex(style); }
    static int CannonEffectShaderMode(CannonEffectStyle style) { return CannonEffectShaderIndex(style); }

private:
    static constexpr int kNameProgramCount = 6;
    static constexpr int kCannonProgramCount = 11;

    struct Program {
        Shader shader{};
        int locTime = -1;
        int locPrimary = -1;
        int locAccent = -1;
        int locResolution = -1;
        int locUvScale = -1;
        bool ready = false;
    };

    std::array<Program, kNameProgramCount> namePrograms_{};
    std::array<Program, kCannonProgramCount> cannonPrograms_{};
    Texture2D textMaskTex_{};
    bool ready_ = false;
    int textMaskAllocW_ = 0;
    int textMaskAllocH_ = 0;
    std::string textMaskCache_;

    bool EnsureTextMask(const char* text, int fontSize, int rw, int rh, int padX, int padY);
    void DrawNameOrnaments(const char* text, int x, int y, int fontSize,
                           NameEffectStyle style, Color primary, Color accent);
    void DrawStyledNameCpu(const char* text, int x, int y, int fontSize,
                           NameEffectStyle style, Color primary, Color accent);

    bool CompileProgram(Program* out, const std::string& common, const std::string& pass,
                        const char* effectRelPath);
    static void UnloadProgram(Program* program);
    static void ApplyUniforms(const Program& program, Color primary, Color accent,
                              float w, float h, float uvScale, float timeScale);
};

extern CosmeticShaders gCosmeticShaders;
