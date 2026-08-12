#pragma once
#include <raylib.h>
#include <vector>
#include "PhysicsWorld.h"
#include "Terrain.h"
#include "Cannon.h"
#include "Projectile.h"
#include "ParticleSystem.h"
#include "AI.h"
#include "Powerup.h"
#include "Localization.h"
#include "Platform.h"

enum class GameMode { PvP, PvAI };
enum class GameVersion { Classic, Plus };
enum class GameState { MainMenu, About, Instructions, Aiming, ProjectileFlying, TurnTransition, RoundOver };
enum class AimPhase { Angle, Power };

class Game {
public:
    Game();
    ~Game();

    void Run();

private:
    void Update(float dt);
    void Draw();

    void UpdateMainMenu();
    void DrawMainMenu();
    void UpdateAbout();
    void DrawAbout();
    void UpdateInstructions();
    void DrawInstructions();

    void StartMatch(GameMode mode);
    void UpdateAiming();
    void UpdateProjectileFlight(float dt);
    void ResolveImpact(Vector2 impactPos, bool hitCannon, Cannon* hitTarget);
    void EndTurn();
    void CheckRoundEnd();

    void DrawHUD();
    void DrawWindIndicator() const;
    void DrawMenuButton() const;
    bool HandleMenuButtonClick(); // retorna true se o clique foi consumido pelo botão
    Color HudTextColor() const { return nightMode ? Color{235, 235, 235, 255} : Color{40, 30, 20, 255}; }
    Color HudTextColorDim() const { return nightMode ? Color{200, 200, 210, 255} : Color{60, 45, 30, 255}; }

    // Calcula o maior valor de aceleração de vento que ainda garante que o
    // alcance máximo (MAX_POWER, ângulo ótimo) supere a distância atual
    // entre os canhões com uma margem de segurança — assim o vento sorteado
    // nunca torna um acerto matematicamente impossível.
    float ComputeSafeMaxWindAccel() const;

    // --- estado geral ---
    GameState state = GameState::MainMenu;
    GameMode  mode  = GameMode::PvAI;
    GameVersion version = GameVersion::Classic;
    Lang language = Lang::PT_BR;

    // --- física / mundo ---
    PhysicsWorld physics;
    Terrain terrain;
    Cannon player1, player2;
    Projectile projectile;
    ParticleSystem particles;
    AI ai;

    int currentPlayer = 1; // 1 ou 2
    float windForce = 0.0f; // força horizontal aplicada ao projétil

    // --- áudio ---
    Sound sndFire{};
    Sound sndExplosion{};
    Music music{};
    bool audioReady = false;

    // --- sprites placeholder ---
    Texture2D texCannonLeft{};
    Texture2D texCannonRight{};
    Texture2D texBackground{};
    Texture2D texBackgroundNight{};
    Texture2D texTerrainTile{};
    Texture2D texProjectile{};
    bool spritesReady = false;
    bool nightMode = false; // sorteado a cada partida

    // --- controle de mira (mecanismo original: oscila e trava no clique) ---
    AimPhase aimPhase = AimPhase::Angle;
    float aimOscTimer = 0.0f;

    // --- transições ---
    float stateTimer = 0.0f;
    enum class RoundOutcome { None, Draw, DrawBuried, P1Wins, P2Wins, P1WinsBuried, P2WinsBuried };
    RoundOutcome roundOutcome = RoundOutcome::None;
    const char* ResolveRoundMessage() const;

    // --- diálogo de confirmação "voltar ao menu" ---
    bool showMenuConfirm = false;
    void DrawMenuConfirmDialog() const;
    void UpdateMenuConfirmDialog(); // consome cliques enquanto o diálogo está aberto

    // --- poeira ambiente (reage à força/direção do vento) ---
    struct DustMote {
        Vector2 pos;
        float depth;   // 0..1: motas "mais perto" são maiores/rápidas/opacas
        float size;
        float alpha;
    };
    std::vector<DustMote> dustMotes;
    void InitDustMotes();
    void UpdateDustMotes(float dt);
    void DrawDustMotes() const;

    void ResetRound(unsigned int seed);

    // --- resolução virtual (render texture) ---
    // O jogo inteiro é desenhado internamente numa resolução fixa
    // (cfg::SCREEN_WIDTH x cfg::SCREEN_HEIGHT) e depois escalada/centralizada
    // pra caber na janela/tela real — essencial no Android, onde a
    // NativeActivity roda sempre em tela cheia na resolução nativa do
    // aparelho (nunca 1280x720). Sem isso, todo o layout (botões, HUD) e a
    // leitura de toque/mouse ficam desalinhados.
    RenderTexture2D virtualScreen{};
    Vector2 GetVirtualMouse() const;
    void DrawVirtualScreenScaled() const;

    // --- power-ups (versão Plus) ---
    std::vector<Powerup> activePowerups;
    int turnsSincePowerupCheck = 0;
    Vector2 prevProjectilePos{}; // usado para checagem de colisão "varrida" (evita atravessar em alta velocidade)
    bool guidedDiving = false; // teleguiado: uma vez que entra na fase de "mergulho" no alvo, nunca mais volta a mirar no ápice (evita oscilação/instabilidade perto do limiar de distância)

    void MaybeSpawnPowerup();
    void DrawPowerup() const;
    void CheckPowerupCollision(Vector2 projFrom, Vector2 projTo);
    void ApplyPowerupEffect(Cannon& picker, PowerupType type);
    void TickPowerupTurnEffects(Cannon& startingTurnCannon); // chamado quando a vez volta pro dono do efeito

    // mensagem flutuante "você pegou X" acima do canhão
    const char* powerupMessageText = nullptr;
    float powerupMessageTimer = 0.0f;
    Vector2 powerupMessagePos{};
    void ShowPowerupMessage(const char* text, Vector2 pos);
    void DrawPowerupMessage() const;
    void DrawPowerupTooltip() const;

    // --- screen shake (versão Plus) ---
    float shakeTimer = 0.0f;
    float shakeDuration = 1.0f;
    float shakeMagnitude = 0.0f;
    void TriggerShake(float magnitudePx, float durationSec);
    Vector2 ComputeShakeOffset() const;

    // --- menu: alternância Classic/Plus ---
    void DrawVersionSwitch(Vector2 mouse);
    void UpdateVersionSwitch(Vector2 mouse);

    // --- menu: alternância de idioma (bandeiras BR/EUA) ---
    void DrawLanguageFlags(Vector2 mouse);
    void UpdateLanguageFlags(Vector2 mouse);

    // --- painel de desenvolvedor oculto (F9) — só existe na build de PC ---
    bool devMode = false; // mantido sempre declarado (custo zero) por simplicidade
#if !CANNON_DUEL_ANDROID_BUILD
    bool UpdateDevPanel(); // retorna true se consumiu o clique deste frame
    void DrawDevPanel() const;
    void DevForceSpawnPowerup();
    void DevGrantPowerupToPlayer1(PowerupType type);
#endif

    // --- botão in-game "resetar linha de ângulo" (PC e Android) ---
    void DrawResetAngleButton() const;
    bool HandleResetAngleButtonClick();
};
