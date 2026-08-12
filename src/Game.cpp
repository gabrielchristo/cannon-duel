#include "Game.h"
#include "Platform.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>

namespace {
// No desktop, os assets ficam numa pasta "assets/" ao lado do executável,
// então os caminhos usados pelo raylib precisam do prefixo "assets/". No
// Android, o raylib carrega arquivos de dentro do APK via AAssetManager,
// cujos caminhos já são relativos à RAIZ da pasta assets/ do APK — incluir
// o prefixo "assets/" de novo faria ele procurar por uma subpasta "assets/"
// dentro de "assets/", que não existe, e o carregamento falha em silêncio
// (é exatamente isso que fazia sprites/sons não aparecerem no Android).
inline std::string AssetPath(const char* relative) {
#if CANNON_DUEL_ANDROID_BUILD
    return relative;
#else
    return std::string("assets/") + relative;
#endif
}

float RandF(float lo, float hi) {
    return lo + static_cast<float>(rand()) / RAND_MAX * (hi - lo);
}
} // namespace

Game::Game() {
    InitWindow(cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, "Cannon Duel");
    SetTargetFPS(cfg::TARGET_FPS);

    // No Android, InitWindow ignora a largura/altura pedidas e usa sempre a
    // resolução nativa da tela do aparelho em tela cheia — por isso
    // renderizamos numa textura de resolução fixa e escalamos na hora de
    // mostrar (ver DrawVirtualScreenScaled / GetVirtualMouse).
    virtualScreen = LoadRenderTexture(cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT);
    SetTextureFilter(virtualScreen.texture, TEXTURE_FILTER_BILINEAR);

    InitAudioDevice();
    audioReady = IsAudioDeviceReady();
    if (audioReady) {
        sndFire      = LoadSound(AssetPath("sounds/fire.ogg").c_str());
        sndExplosion = LoadSound(AssetPath("sounds/explosion.ogg").c_str());

        music = LoadMusicStream(AssetPath("sounds/music.ogg").c_str());
        music.looping = true;
        if (music.frameCount > 0) {
            SetMusicVolume(music, 0.5f);
        }
    }

    texCannonLeft   = LoadTexture(AssetPath("sprites/cannon_left.png").c_str());
    texCannonRight  = LoadTexture(AssetPath("sprites/cannon_right.png").c_str());
    texBackground   = LoadTexture(AssetPath("sprites/background.png").c_str());
    texBackgroundNight = LoadTexture(AssetPath("sprites/background_night.png").c_str());
    texTerrainTile  = LoadTexture(AssetPath("sprites/terrain_tile.png").c_str());
    texProjectile   = LoadTexture(AssetPath("sprites/projectile.png").c_str());
    spritesReady = (texCannonLeft.id != 0 && texCannonRight.id != 0);

    srand(static_cast<unsigned int>(time(nullptr)));
}

Game::~Game() {
    if (audioReady) {
        UnloadSound(sndFire);
        UnloadSound(sndExplosion);
        UnloadMusicStream(music);
        CloseAudioDevice();
    }
    UnloadTexture(texCannonLeft);
    UnloadTexture(texCannonRight);
    UnloadTexture(texBackground);
    UnloadTexture(texBackgroundNight);
    UnloadTexture(texTerrainTile);
    UnloadTexture(texProjectile);
    UnloadRenderTexture(virtualScreen);
    CloseWindow();
}

void Game::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
}

Vector2 Game::GetVirtualMouse() const {
    Vector2 mouse = GetMousePosition();
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float scale = std::min(screenW / cfg::SCREEN_WIDTH, screenH / cfg::SCREEN_HEIGHT);
    float offsetX = (screenW - cfg::SCREEN_WIDTH * scale) * 0.5f;
    float offsetY = (screenH - cfg::SCREEN_HEIGHT * scale) * 0.5f;

    Vector2 v;
    v.x = (mouse.x - offsetX) / scale;
    v.y = (mouse.y - offsetY) / scale;
    v.x = std::clamp(v.x, 0.0f, static_cast<float>(cfg::SCREEN_WIDTH));
    v.y = std::clamp(v.y, 0.0f, static_cast<float>(cfg::SCREEN_HEIGHT));
    return v;
}

void Game::DrawVirtualScreenScaled() const {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float scale = std::min(screenW / cfg::SCREEN_WIDTH, screenH / cfg::SCREEN_HEIGHT);

    Rectangle src = { 0, 0, static_cast<float>(virtualScreen.texture.width),
                      -static_cast<float>(virtualScreen.texture.height) }; // Y invertido (render texture)
    Rectangle dst = {
        (screenW - cfg::SCREEN_WIDTH * scale) * 0.5f,
        (screenH - cfg::SCREEN_HEIGHT * scale) * 0.5f,
        cfg::SCREEN_WIDTH * scale,
        cfg::SCREEN_HEIGHT * scale
    };
    DrawTexturePro(virtualScreen.texture, src, dst, {0, 0}, 0.0f, WHITE);
}

// ---------------------------------------------------------------------------
// Setup de partida
// ---------------------------------------------------------------------------
float Game::ComputeSafeMaxWindAccel() const {
    // Alcance máximo (sem vento) na potência máxima, ângulo ótimo de 45°:
    //   R = v² / g
    // Com vento constante 'a' contra a trajetória a 45°, o alcance efetivo é:
    //   x(T) = (v²/g) * (1 - a/g)
    // (derivado de x(T) = v*cos45*T - 0.5*a*T², T = v*sqrt(2)/g)
    //
    // Queremos garantir x(T) >= D * safetyFactor, ou seja, mesmo no pior
    // vento possível (soprando contra, o tempo todo, no ângulo mais
    // desfavorável) ainda sobra alcance para acertar com folga — cobrindo
    // imprecisões de ângulo/tempo de clique do jogador.
    float distPx = std::fabs(player2.x - player1.x);
    float D = cfg::PxToM(distPx);
    float v = cfg::MAX_POWER;
    float g = cfg::GRAVITY_MPS2;
    constexpr float safetyFactor = 1.25f; // exige 25% de alcance extra de folga

    float requiredRange = D * safetyFactor;
    float maxRangeNoWind = (v * v) / g;

    if (maxRangeNoWind <= requiredRange) {
        // Config atual (potência/distância) já não teria folga nem sem
        // vento; não há aceleração de vento segura — melhor não arriscar.
        return 0.0f;
    }

    float a = g - requiredRange * (g * g) / (v * v);
    return std::max(0.0f, a);
}

void Game::ResetRound(unsigned int seed) {
    terrain.GenerateRandom(seed);
    terrain.RebuildPhysicsBody(physics.Id());

    float leftX  = cfg::CANNON_MARGIN_PX;
    float rightX = cfg::SCREEN_WIDTH - cfg::CANNON_MARGIN_PX;

    player1.Init(leftX,  terrain.HeightAt(leftX),  CannonSide::Left);
    player2.Init(rightX, terrain.HeightAt(rightX), CannonSide::Right);

    currentPlayer = 1;
    windForce = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
    nightMode = (rand() % 2) == 0; // cenário dia/noite sorteado a cada partida

    activePowerups.clear();
    turnsSincePowerupCheck = 0;
    powerupMessageTimer = 0.0f;
    shakeTimer = 0.0f;

    state = GameState::Aiming;
}

void Game::StartMatch(GameMode m) {
    mode = m;
    if (mode == GameMode::PvAI) ai.SetDifficulty(0.55f);
    ResetRound(static_cast<unsigned int>(time(nullptr)) ^ rand());

    // Música toca em loop só durante a partida (não no menu).
    if (audioReady && music.frameCount > 0) {
        PlayMusicStream(music);
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
const char* Game::ResolveRoundMessage() const {
    switch (roundOutcome) {
        case RoundOutcome::Draw: return T(TK::RoundDraw, language);
        case RoundOutcome::DrawBuried: return T(TK::RoundDrawBuried, language);
        case RoundOutcome::P1Wins: return T(TK::RoundP1Wins, language);
        case RoundOutcome::P2Wins: return T(TK::RoundP2Wins, language);
        case RoundOutcome::P1WinsBuried: return T(TK::RoundP1WinsBuried, language);
        case RoundOutcome::P2WinsBuried: return T(TK::RoundP2WinsBuried, language);
        default: return "";
    }
}

void Game::Update(float dt) {
    // Precisa ser chamado todo frame pro streaming da música avançar (e
    // fazer o loop) — independe de qualquer outro estado/painel.
    if (audioReady && music.frameCount > 0) {
        UpdateMusicStream(music);
    }

#if !CANNON_DUEL_ANDROID_BUILD
    // Painel de desenvolvedor oculto: F9 alterna a visibilidade. Só existe
    // na build de PC — depende de teclado físico e é uma ferramenta de
    // testes internos, sem sentido numa build mobile/touch.
    if (IsKeyPressed(KEY_F9)) {
        devMode = !devMode;
    }
    if (devMode && UpdateDevPanel()) {
        return;
    }
#endif

    // Diálogo de confirmação tem prioridade máxima: enquanto aberto, nenhum
    // outro input do jogo é processado.
    if (showMenuConfirm) {
        UpdateMenuConfirmDialog();
        return;
    }

    // Botão "voltar ao menu" abre o diálogo de confirmação em vez de sair
    // direto — evita perder uma partida em andamento por clique acidental.
    if (state != GameState::MainMenu && state != GameState::About && HandleMenuButtonClick()) {
        showMenuConfirm = true;
        return;
    }

    // Botão in-game "resetar linha de ângulo" — só faz sentido durante a
    // mira de um jogador humano (não durante o turno da IA). Equivalente
    // touch-friendly do atalho de teclado B (PC), mas disponível nas duas
    // plataformas.
    bool humanAimingTurn = (state == GameState::Aiming && !(mode == GameMode::PvAI && currentPlayer == 2));
    if (humanAimingTurn && HandleResetAngleButtonClick()) {
        if (aimPhase == AimPhase::Angle) {
            aimOscTimer = 0.0f;
        } else {
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
        }
        return;
    }

    // Atualiza as partículas em TODOS os estados (exceto o menu, onde não há
    // partida em andamento). Antes isso só rodava durante ProjectileFlying/
    // RoundOver, então uma explosão ficava "congelada" na tela durante a
    // transição de turno e a mira do próximo jogador, só sumindo quando o
    // próximo tiro fosse disparado.
    if (state != GameState::MainMenu) {
        particles.Update(dt);
    }

    if (shakeTimer > 0.0f) shakeTimer = std::max(0.0f, shakeTimer - dt);
    if (powerupMessageTimer > 0.0f) powerupMessageTimer = std::max(0.0f, powerupMessageTimer - dt);

    switch (state) {
        case GameState::MainMenu:
            UpdateMainMenu();
            break;
        case GameState::About:
            UpdateAbout();
            break;
        case GameState::Aiming:
            UpdateAiming();
            break;
        case GameState::ProjectileFlying:
            UpdateProjectileFlight(dt);
            break;
        case GameState::TurnTransition:
            stateTimer -= dt;
            if (stateTimer <= 0.0f) state = GameState::Aiming;
            break;
        case GameState::RoundOver:
            stateTimer -= dt;
            if (stateTimer <= 0.0f && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (audioReady) StopMusicStream(music);
                state = GameState::MainMenu;
            }
            break;
    }
}

void Game::UpdateMainMenu() {
    Vector2 m = GetVirtualMouse();
    UpdateVersionSwitch(m);
    UpdateLanguageFlags(m);

    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 330, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 406, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 482, 280, 56 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, btn1P)) StartMatch(GameMode::PvAI);
        else if (CheckCollisionPointRec(m, btn2P)) StartMatch(GameMode::PvP);
        else if (CheckCollisionPointRec(m, btnAbout)) state = GameState::About;
    }
}

void Game::UpdateAbout() {
    Vector2 m = GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 100.0f, 200, 52 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, backBtn)) {
        state = GameState::MainMenu;
    }
}

// ---------------------------------------------------------------------------
// Power-ups (versão Plus)
// ---------------------------------------------------------------------------
void Game::MaybeSpawnPowerup() {
    if (static_cast<int>(activePowerups.size()) >= cfg::POWERUP_MAX_ACTIVE) return;
    if (turnsSincePowerupCheck < cfg::POWERUP_SPAWN_EVERY_TURNS) return;
    turnsSincePowerupCheck = 0;

    // posição aleatória, evitando ficar em cima dos canhões
    float margin = 160.0f;
    float x = RandF(margin, cfg::SCREEN_WIDTH - margin);

    // sorteio ponderado (Guiado é mais raro)
    struct Entry { PowerupType type; float weight; };
    Entry entries[] = {
        { PowerupType::DoubleDamage,      cfg::POWERUP_WEIGHT_DOUBLE_DMG },
        { PowerupType::TrajectoryPreview, cfg::POWERUP_WEIGHT_TRAJECTORY },
        { PowerupType::Guided,            cfg::POWERUP_WEIGHT_GUIDED },
        { PowerupType::Heal,              cfg::POWERUP_WEIGHT_HEAL },
        { PowerupType::Shield,            cfg::POWERUP_WEIGHT_SHIELD },
    };
    float totalWeight = 0.0f;
    for (auto& e : entries) totalWeight += e.weight;
    float roll = RandF(0.0f, totalWeight);
    PowerupType chosen = entries[0].type;
    for (auto& e : entries) {
        if (roll < e.weight) { chosen = e.type; break; }
        roll -= e.weight;
    }

    Powerup p;
    p.active = true;
    p.type = chosen;
    p.x = x;
    activePowerups.push_back(p);
}

void Game::DrawPowerup() const {
    for (const auto& pu : activePowerups) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        Color c = PowerupColor(pu.type);
        DrawCircleV(center, cfg::POWERUP_RADIUS_PX, c);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                         static_cast<int>(cfg::POWERUP_RADIUS_PX), Color{20, 20, 20, 255});
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                         static_cast<int>(cfg::POWERUP_RADIUS_PX) + 3, Fade(c, 0.4f));

        const char* label = PowerupLabel(pu.type);
        int fs = 14;
        int tw = MeasureText(label, fs);
        DrawText(label, static_cast<int>(center.x - tw / 2), static_cast<int>(center.y - fs / 2), fs, WHITE);
    }
}

void Game::DrawPowerupTooltip() const {
    Vector2 mouse = GetVirtualMouse();

    for (const auto& pu : activePowerups) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        float dist = std::sqrt(std::pow(mouse.x - center.x, 2) + std::pow(mouse.y - center.y, 2));
        if (dist > cfg::POWERUP_RADIUS_PX + 10.0f) continue;

        const char* desc = PowerupDescription(pu.type, language);
        int fs = 15;
        int tw = MeasureText(desc, fs);
        float boxW = tw + 16.0f, boxH = fs + 12.0f;

        float bx = mouse.x + 16.0f;
        float by = mouse.y - boxH - 10.0f;
        // mantém a tooltip inteira dentro da tela
        bx = std::clamp(bx, 4.0f, static_cast<float>(cfg::SCREEN_WIDTH) - boxW - 4.0f);
        by = std::clamp(by, 4.0f, static_cast<float>(cfg::SCREEN_HEIGHT) - boxH - 4.0f);

        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                      Fade(Color{20, 20, 20, 255}, 0.9f));
        DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                            PowerupColor(pu.type));
        DrawText(desc, static_cast<int>(bx + 8), static_cast<int>(by + 6), fs, WHITE);
        break; // só uma tooltip por vez, mesmo com vários power-ups próximos
    }
}

void Game::CheckPowerupCollision(Vector2 projFrom, Vector2 projTo) {
    Vector2 seg = { projTo.x - projFrom.x, projTo.y - projFrom.y };
    float segLenSq = seg.x * seg.x + seg.y * seg.y;
    float hitRadius = cfg::POWERUP_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + cfg::POWERUP_HIT_TOLERANCE_PX;

    for (auto& pu : activePowerups) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f };

        // Distância do PONTO ao SEGMENTO (projFrom -> projTo), em vez de só
        // checar a posição atual do projétil: em alta velocidade, a bala
        // pode "pular" de um lado do power-up para o outro entre dois
        // frames de física sem que nenhuma das duas posições pontuais
        // esteja dentro do raio — mesmo que a trajetória tenha cruzado por
        // cima dele. Checar o segmento inteiro percorrido no frame evita
        // esse "vazamento".
        float t = 0.0f;
        if (segLenSq > 0.0001f) {
            t = ((center.x - projFrom.x) * seg.x + (center.y - projFrom.y) * seg.y) / segLenSq;
            t = std::clamp(t, 0.0f, 1.0f);
        }
        Vector2 closest = { projFrom.x + seg.x * t, projFrom.y + seg.y * t };
        float dist = std::sqrt(std::pow(closest.x - center.x, 2) + std::pow(closest.y - center.y, 2));

        if (dist > hitRadius) continue;

        Cannon& shooter = (currentPlayer == 1) ? player1 : player2;
        PowerupType type = pu.type;
        pu.active = false;
        ApplyPowerupEffect(shooter, type);
        break; // um power-up por frame é suficiente
    }

    // remove os já consumidos
    activePowerups.erase(std::remove_if(activePowerups.begin(), activePowerups.end(),
                          [](const Powerup& p) { return !p.active; }),
                          activePowerups.end());
}

void Game::ApplyPowerupEffect(Cannon& picker, PowerupType type) {
    switch (type) {
        case PowerupType::DoubleDamage:
            // só vira ativo no PRÓXIMO tiro (não no que acabou de coletar o
            // power-up) — ver promoção em ResolveImpact.
            picker.queuedDoubleDamage = true;
            break;
        case PowerupType::TrajectoryPreview:
            picker.trajectoryPreviewTurnsLeft = cfg::POWERUP_TRAJECTORY_TURNS;
            break;
        case PowerupType::Guided:
            picker.pendingGuided = true;
            break;
        case PowerupType::Heal: {
            float amount = RandF(cfg::POWERUP_HEAL_MIN_RATIO, cfg::POWERUP_HEAL_MAX_RATIO) * cfg::CANNON_MAX_HEALTH;
            picker.health = std::min(cfg::CANNON_MAX_HEALTH, picker.health + amount);
            break;
        }
        case PowerupType::Shield:
            picker.shieldTurnsLeft = cfg::POWERUP_SHIELD_TURNS;
            break;
        default: break;
    }

    Vector2 base = { picker.x, picker.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f - 40.0f };
    ShowPowerupMessage(PowerupDescription(type, language), base);
}

void Game::TickPowerupTurnEffects(Cannon& startingTurnCannon) {
    if (startingTurnCannon.shieldTurnsLeft > 0) startingTurnCannon.shieldTurnsLeft--;
    if (startingTurnCannon.trajectoryPreviewTurnsLeft > 0) startingTurnCannon.trajectoryPreviewTurnsLeft--;
}

void Game::ShowPowerupMessage(const char* text, Vector2 pos) {
    powerupMessageText = text;
    powerupMessagePos = pos;
    powerupMessageTimer = cfg::POWERUP_MESSAGE_DURATION_SEC;
}

void Game::DrawPowerupMessage() const {
    if (powerupMessageTimer <= 0.0f || !powerupMessageText) return;

    float alpha = std::min(1.0f, powerupMessageTimer / 0.4f); // fade-out no final
    int fs = 18;
    int tw = MeasureText(powerupMessageText, fs);

    float boxPad = 6.0f;
    float boxW = tw + boxPad * 2.0f;
    float px = powerupMessagePos.x - tw / 2.0f;

    // garante que a caixa inteira (não só o texto) fique dentro da janela,
    // mesmo quando o canhão está perto da borda esquerda/direita da tela
    float minX = boxPad;
    float maxX = static_cast<float>(cfg::SCREEN_WIDTH) - boxW + boxPad;
    px = std::clamp(px, minX, maxX);

    float py = std::clamp(powerupMessagePos.y, 4.0f, static_cast<float>(cfg::SCREEN_HEIGHT) - fs - 8.0f);

    DrawRectangle(static_cast<int>(px) - static_cast<int>(boxPad), static_cast<int>(py) - 4,
                  static_cast<int>(boxW), fs + 8, Fade(Color{20, 20, 20, 255}, alpha * 0.75f));
    DrawText(powerupMessageText, static_cast<int>(px), static_cast<int>(py), fs,
             Fade(Color{255, 230, 140, 255}, alpha));
}

// ---------------------------------------------------------------------------
// Screen shake (versão Plus)
// ---------------------------------------------------------------------------
void Game::TriggerShake(float magnitudePx, float durationSec) {
    if (version != GameVersion::Plus) return;
    shakeMagnitude = magnitudePx;
    shakeDuration = durationSec;
    shakeTimer = durationSec;
}

Vector2 Game::ComputeShakeOffset() const {
    if (shakeTimer <= 0.0f) return { 0.0f, 0.0f };
    float ratio = shakeTimer / shakeDuration;
    float mag = shakeMagnitude * ratio;
    return { RandF(-mag, mag), RandF(-mag, mag) };
}

// ---------------------------------------------------------------------------
// Painel de desenvolvedor oculto (F9) — não é exposto em nenhum menu normal.
// Serve pra testar rapidamente todas as funcionalidades sem depender de RNG
// (vento, spawn de power-up) ou de sobreviver várias rodadas.
// Compilado SOMENTE na build de PC: além de depender de teclado (F9), é uma
// ferramenta interna de desenvolvimento que não faz sentido existir — nem
// ocupar espaço/binário — numa build mobile.
// ---------------------------------------------------------------------------
#if !CANNON_DUEL_ANDROID_BUILD
void Game::DevForceSpawnPowerup() {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    turnsSincePowerupCheck = cfg::POWERUP_SPAWN_EVERY_TURNS;
    MaybeSpawnPowerup();
}

void Game::DevGrantPowerupToPlayer1(PowerupType type) {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    ApplyPowerupEffect(player1, type);
}

bool Game::UpdateDevPanel() {
    Vector2 m = GetVirtualMouse();
    Rectangle panel = { 16, 90, 240, 505 };
    if (!CheckCollisionPointRec(m, panel)) return false;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return true; // dentro do painel, sem clique: ainda consome (evita vazar clique pro jogo)

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    float y = panel.y + 34;
    float bw = panel.width - 20, bh = 28, gap = 5;
    auto hit = [&](float rowY) {
        Rectangle r = { panel.x + 10, rowY, bw, bh };
        return CheckCollisionPointRec(m, r);
    };

    if (hit(y) && inMatch) { player1.health = cfg::CANNON_MAX_HEALTH; player2.health = cfg::CANNON_MAX_HEALTH; return true; }
    y += bh + gap;
    if (hit(y) && inMatch) { DevForceSpawnPowerup(); return true; }
    y += bh + gap;
    if (hit(y) && inMatch) { windForce = 0.0f; return true; }
    y += bh + gap;
    if (hit(y) && inMatch && state == GameState::Aiming) { EndTurn(); return true; }
    y += bh + gap;
    if (hit(y)) { version = (version == GameVersion::Classic) ? GameVersion::Plus : GameVersion::Classic; return true; }
    y += bh + gap;
    if (hit(y)) { nightMode = !nightMode; return true; }
    y += bh + gap;
    if (hit(y) && state == GameState::MainMenu) { StartMatch(GameMode::PvAI); return true; }
    y += bh + gap;

    // seção: conceder power-up específico ao Jogador 1
    y += 22; // espaço do subtítulo
    PowerupType types[] = { PowerupType::DoubleDamage, PowerupType::TrajectoryPreview,
                             PowerupType::Guided,
                             PowerupType::Heal, PowerupType::Shield };
    for (PowerupType t : types) {
        if (hit(y) && inMatch) { DevGrantPowerupToPlayer1(t); return true; }
        y += bh + gap;
    }

    return true;
}

void Game::DrawDevPanel() const {
    Rectangle panel = { 16, 90, 240, 505 };
    DrawRectangleRec(panel, Fade(Color{15, 15, 20, 255}, 0.9f));
    DrawRectangleLinesEx(panel, 2, Color{255, 210, 60, 255});

    const char* title = "DEV PANEL (F9)";
    DrawText(title, static_cast<int>(panel.x + 10), static_cast<int>(panel.y + 6), 16, Color{255, 210, 60, 255});

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    Vector2 m = GetVirtualMouse();
    float y = panel.y + 34;
    float bw = panel.width - 20, bh = 28, gap = 5;

    auto drawRow = [&](const char* label, bool enabled, bool highlight = false) {
        Rectangle r = { panel.x + 10, y, bw, bh };
        bool hover = enabled && CheckCollisionPointRec(m, r);
        Color bg = !enabled ? Color{40, 40, 45, 255}
                 : highlight ? Color{80, 160, 90, 255}
                 : hover ? Color{90, 90, 100, 255} : Color{55, 55, 65, 255};
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, Color{200, 200, 210, 150});
        Color txt = enabled ? WHITE : Color{130, 130, 135, 255};
        DrawText(label, static_cast<int>(r.x + 8), static_cast<int>(r.y + 6), 13, txt);
        y += bh + gap;
    };

    drawRow("Curar os dois canhoes", inMatch);
    drawRow("Forcar spawn de power-up", inMatch);
    drawRow("Zerar vento", inMatch);
    drawRow("Pular turno", inMatch && state == GameState::Aiming);
    drawRow(version == GameVersion::Plus ? "Versao: PLUS" : "Versao: CLASSIC", true);
    drawRow(nightMode ? "Cenario: NOITE" : "Cenario: DIA", true);
    drawRow("Iniciar partida rapida (1J)", state == GameState::MainMenu);

    y += 22;
    DrawText("Dar power-up ao Jogador 1:", static_cast<int>(panel.x + 10), static_cast<int>(y - 18), 13,
              Color{200, 200, 210, 255});

    const char* labels[] = { "2x Dano em dobro", "Trajetoria prevista",
                              "Teleguiado", "Cura", "Escudo" };
    for (const char* label : labels) {
        drawRow(label, inMatch);
    }
}
#endif // !CANNON_DUEL_ANDROID_BUILD

void Game::UpdateAiming() {
    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    Cannon& other  = (currentPlayer == 1) ? player2 : player1;

    bool isAITurn = (mode == GameMode::PvAI && currentPlayer == 2);

    if (isAITurn) {
        // IA "pensa" e atira quase imediatamente (poderia adicionar delay/timer)
        float targetX = other.x;
        float targetY = other.groundY;

        // Versão Plus: a IA às vezes prefere mirar num power-up no mapa em
        // vez de atacar o adversário diretamente — com prioridade maior
        // quando está com pouca vida (cura/escudo) e uma chance geral menor
        // pros demais casos, pra não ficar sempre ignorando o adversário.
        if (version == GameVersion::Plus && !activePowerups.empty()) {
            float healthRatio = active.health / cfg::CANNON_MAX_HEALTH;
            int chosenIdx = -1;
            float bestPriority = 0.0f;

            for (size_t i = 0; i < activePowerups.size(); ++i) {
                const Powerup& pu = activePowerups[i];
                float priority = 0.25f; // chance-base de considerar qualquer power-up

                if (healthRatio < 0.45f &&
                    (pu.type == PowerupType::Heal || pu.type == PowerupType::Shield)) {
                    priority = 0.85f; // prioridade alta quando a vida está baixa
                } else if (pu.type == PowerupType::DoubleDamage || pu.type == PowerupType::Guided) {
                    priority = 0.4f; // vantagens ofensivas valem um pouco mais que a base
                }

                if (priority > bestPriority) {
                    bestPriority = priority;
                    chosenIdx = static_cast<int>(i);
                }
            }

            if (chosenIdx >= 0 && RandF(0.0f, 1.0f) < bestPriority) {
                targetX = activePowerups[static_cast<size_t>(chosenIdx)].x;
                targetY = terrain.HeightAt(targetX);
            }
        }

        ai.ComputeShot(active, targetX, targetY, windForce);

        Vector2 muzzle = active.MuzzlePosition();
        // Teleguiado sempre sai com um ângulo mínimo elevado, garantindo que
        // comece subindo (arco por cima) em vez de eventualmente sair quase
        // reto e bater no terreno próximo antes da correção de rota conseguir agir.
        Vector2 dir = (version == GameVersion::Plus && active.pendingGuided)
            ? active.DirectionAtAngle(std::max(active.angleDeg, 55.0f))
            : active.AimDirection();
        projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
        prevProjectilePos = muzzle;
        if (audioReady) PlaySound(sndFire);
        aimPhase = AimPhase::Angle;
        state = GameState::ProjectileFlying;
        return;
    }

    // ---- Mecanismo original: oscila e trava no clique ----
    aimOscTimer += GetFrameTime();

    // Atalhos de teclado (espaço = confirmar/atirar, B = resetar/voltar ao
    // ângulo) só existem na build de PC — foram pensados para permitir que,
    // no modo 2 jogadores, um jogador use o mouse e o outro o teclado,
    // compartilhando a mesma tela. Numa build mobile/touch os dois jogadores
    // compartilham a tela de toque, então esses atalhos não fazem sentido e
    // o botão in-game de resetar ângulo (touch-friendly) cobre essa função.
#if CANNON_DUEL_ANDROID_BUILD
    bool confirmPressed = false;
    bool resetPressed = false;
#else
    bool confirmPressed = IsKeyPressed(KEY_SPACE);
    bool resetPressed = IsKeyPressed(KEY_B);
#endif

    if (aimPhase == AimPhase::Angle) {
        // -90° (reto pra baixo) a 90° (reto pra cima), oscilando continuamente
        // (onda triangular), na direção do oponente. Com "trajetória
        // prevista" ativa, oscila mais devagar também — senão o preview não
        // ajuda muito, já que o timing fica apertado demais em ambas as fases.
        float period = cfg::ANGLE_OSC_PERIOD_SEC *
            ((version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0)
                ? cfg::POWERUP_TRAJECTORY_POWER_SLOWDOWN : 1.0f);
        float t = fmodf(aimOscTimer, period) / period; // 0..1
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
        float angle = -90.0f + tri * 180.0f;
        active.SetAim(angle, active.power01);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || confirmPressed) {
            aimPhase = AimPhase::Power;
            aimOscTimer = 0.0f;
        }
        if (resetPressed) {
            // reseta a linha de ângulo, reiniciando a oscilação do começo
            aimOscTimer = 0.0f;
        }
    } else { // AimPhase::Power
        // Com "trajetória prevista" ativa, a barra oscila mais devagar —
        // senão o preview não ajuda muito, já que o timing fica apertado demais.
        float period = cfg::POWER_OSC_PERIOD_SEC *
            ((version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0)
                ? cfg::POWERUP_TRAJECTORY_POWER_SLOWDOWN : 1.0f);
        float t = fmodf(aimOscTimer, period) / period; // 0..1
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
        active.SetAim(active.angleDeg, tri);

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || resetPressed) {
            // volta para a seleção de ângulo, começando a oscilação do zero
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            return;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || confirmPressed) {
            Vector2 muzzle = active.MuzzlePosition();
            Vector2 dir = (version == GameVersion::Plus && active.pendingGuided)
                ? active.DirectionAtAngle(std::max(active.angleDeg, 55.0f))
                : active.AimDirection();
            projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
            prevProjectilePos = muzzle;
            if (audioReady) PlaySound(sndFire);
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            state = GameState::ProjectileFlying;
        }
    }
}

void Game::UpdateProjectileFlight(float dt) {
    Cannon& shooter = (currentPlayer == 1) ? player1 : player2;
    Cannon& opponent = (currentPlayer == 1) ? player2 : player1;

    projectile.ApplyWind(windForce);

    if (version == GameVersion::Plus && shooter.pendingGuided) {
        // Enquanto o projétil ainda está horizontalmente longe do alvo, mira
        // num ponto alto no céu (força a subir e fazer um arco por cima).
        // Só passa a mirar diretamente no canhão adversário quando já está
        // perto o suficiente para "mergulhar" sobre ele — assim o tiro nunca
        // vai em linha reta baixa e explode sem causar dano no terreno mais
        // próximo antes de chegar perto do alvo.
        Vector2 projPos = projectile.PositionPx();
        float horizDist = std::fabs(projPos.x - opponent.x);
        Vector2 targetPos = (horizDist > cfg::POWERUP_GUIDED_DIVE_DIST_PX)
            ? Vector2{ opponent.x, cfg::POWERUP_GUIDED_APEX_Y_PX }
            : Vector2{ opponent.x, opponent.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
        projectile.ApplyGuidance(targetPos, cfg::POWERUP_GUIDED_TURN_RATE_DEG, dt);
    }

    physics.Step(dt);

    if (!projectile.IsActive()) return;

    Vector2 pos = projectile.PositionPx();

    Color trailColor = (version == GameVersion::Plus && shooter.pendingDoubleDamage)
        ? Color{255, 130, 40, 255}  // rastro em chamas (dano em dobro)
        : Color{235, 230, 215, 255};
    particles.EmitTrail(pos, projectile.VelocityPx(), trailColor);

    if (version == GameVersion::Plus) {
        CheckPowerupCollision(prevProjectilePos, pos);
    }
    prevProjectilePos = pos;

    // fora da tela (nunca deveria bater em nada) -> encerra o turno
    if (pos.x < -50 || pos.x > cfg::SCREEN_WIDTH + 50 || pos.y > cfg::SCREEN_HEIGHT + 200) {
        projectile.Destroy();
        EndTurn();
        return;
    }

    // Colisão com o canhão adversário é checada ANTES do terreno: se o
    // canhão estiver parcialmente afundado (terreno destruído ao redor),
    // um acerto direto deve continuar contando como acerto direto, não como
    // um simples impacto de terreno (senão perderíamos, por exemplo, o
    // bônus de dano em dobro em impacto direto).
    Cannon& target = (currentPlayer == 1) ? player2 : player1;
    Vector2 targetBase = { target.x, target.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
    float distToTarget = std::sqrt(std::pow(pos.x - targetBase.x, 2) + std::pow(pos.y - targetBase.y, 2));

    // Teleguiado: raio de acerto um pouco mais generoso e, se o projétil já
    // está bem próximo do adversário (mesmo que ele esteja afundado no
    // terreno e o ponto tecnicamente "bata" no terreno primeiro), garante
    // que o resultado seja sempre um acerto direto no canhão — é o próprio
    // propósito do power-up ("sempre acerta").
    bool guidedForcedHit = false;
    if (version == GameVersion::Plus && shooter.pendingGuided) {
        float guidedHitRadius = cfg::CANNON_BODY_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + 10.0f;
        guidedForcedHit = (distToTarget <= guidedHitRadius);
    }

    if (guidedForcedHit) {
        ResolveImpact(targetBase, true, &target);
        return;
    }
    if (distToTarget <= cfg::CANNON_BODY_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX) {
        ResolveImpact(pos, true, &target);
        return;
    }

    // colisão com terreno (heightmap)
    if (terrain.IsPointInside(pos.x, pos.y)) {
        ResolveImpact(pos, false, nullptr);
        return;
    }
}

void Game::ResolveImpact(Vector2 impactPos, bool hitCannon, Cannon* hitTarget) {
    projectile.Destroy();
    if (audioReady) PlaySound(sndExplosion);

    // Multiplicadores de dano/raio vindos de power-ups do atirador (versão Plus)
    Cannon& shooter = (currentPlayer == 1) ? player1 : player2;
    float damageMult = 1.0f;
    float radiusMult = 1.0f;
    bool wasDoubleDamage = false, wasGuided = false;

    if (version == GameVersion::Plus) {
        if (shooter.pendingDoubleDamage) { damageMult *= cfg::POWERUP_DOUBLE_DAMAGE_MULT; wasDoubleDamage = true; }
        if (shooter.pendingGuided) { damageMult *= cfg::POWERUP_GUIDED_DAMAGE_MULT; wasGuided = true; }

        // efeitos de um único tiro são consumidos agora, tenha acertado ou não
        shooter.pendingDoubleDamage = false;
        shooter.pendingGuided = false;

        // Se o dano em dobro foi coletado durante ESTE tiro (queuedDoubleDamage),
        // ele só vira ativo a partir do PRÓXIMO tiro — nunca no que acabou de
        // resolver, mesmo que tenha sido esse mesmo projétil a pegar o item.
        if (shooter.queuedDoubleDamage) {
            shooter.pendingDoubleDamage = true;
            shooter.queuedDoubleDamage = false;
        }
    }

    float craterRadius = cfg::CRATER_RADIUS_PX * radiusMult;
    float explosionRadius = cfg::EXPLOSION_RADIUS_PX * radiusMult;
    int particleCount = 50;

    particles.EmitExplosion(impactPos, particleCount);
    terrain.Explode(impactPos.x, impactPos.y, craterRadius);

    if (version == GameVersion::Plus) {
        TriggerShake(hitCannon ? cfg::SHAKE_MAGNITUDE_DIRECT_PX : cfg::SHAKE_MAGNITUDE_TERRAIN_PX,
                      hitCannon ? cfg::SHAKE_DURATION_DIRECT_SEC : cfg::SHAKE_DURATION_TERRAIN_SEC);
    }
    (void)wasGuided;

    // dano em área para os dois canhões, ponderado pela distância
    Cannon* cannons[2] = { &player1, &player2 };
    for (Cannon* c : cannons) {
        Vector2 base = { c->x, c->groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
        float dist = std::sqrt(std::pow(impactPos.x - base.x, 2) + std::pow(impactPos.y - base.y, 2));
        if (dist <= explosionRadius) {
            float falloff = 1.0f - (dist / explosionRadius);
            float dmg = cfg::EXPLOSION_DAMAGE_MAX * falloff * damageMult;
            if (hitCannon && c == hitTarget) dmg = cfg::EXPLOSION_DAMAGE_MAX * damageMult; // impacto direto

            // escudo (power-up) bloqueia todo o dano enquanto ativo
            if (c->shieldTurnsLeft > 0) dmg = 0.0f;

            c->TakeDamage(dmg);
        }
    }

    // canhões podem "afundar" se o chão embaixo deles foi cavado
    player1.groundY = terrain.HeightAt(player1.x);
    player2.groundY = terrain.HeightAt(player2.x);

    // Vitória imediata: se o terreno abaixo de um canhão foi completamente
    // destruído (ele deixaria de ser visível na tela), o outro jogador vence
    // na hora, independente da vida restante.
    bool p1Buried = terrain.IsFullyGone(player1.x);
    bool p2Buried = terrain.IsFullyGone(player2.x);
    if (p1Buried || p2Buried) {
        if (p1Buried) player1.health = 0.0f;
        if (p2Buried) player2.health = 0.0f;

        roundOutcome = (p1Buried && p2Buried) ? RoundOutcome::DrawBuried
                       : (p1Buried ? RoundOutcome::P2WinsBuried : RoundOutcome::P1WinsBuried);
        state = GameState::RoundOver;
        stateTimer = 1.0f;
        return;
    }

    CheckRoundEnd();
}

void Game::CheckRoundEnd() {
    if (!player1.IsAlive() || !player2.IsAlive()) {
        roundOutcome = (!player1.IsAlive() && !player2.IsAlive()) ? RoundOutcome::Draw
                       : (!player1.IsAlive() ? RoundOutcome::P2Wins : RoundOutcome::P1Wins);
        state = GameState::RoundOver;
        stateTimer = 1.0f; // pequena trava antes de aceitar clique para voltar ao menu
        return;
    }
    EndTurn();
}

void Game::EndTurn() {
    if (state == GameState::RoundOver) return;
    currentPlayer = (currentPlayer == 1) ? 2 : 1;

    // sorteia um novo vento a cada turno (como no jogo original), limitado a
    // um valor que ainda garanta ser possível acertar o adversário mesmo no
    // pior caso (ver ComputeSafeMaxWindAccel)
    windForce = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;

    if (version == GameVersion::Plus) {
        Cannon& startingCannon = (currentPlayer == 1) ? player1 : player2;
        TickPowerupTurnEffects(startingCannon);

        turnsSincePowerupCheck++;
        MaybeSpawnPowerup();
    }

    stateTimer = 0.4f;
    state = GameState::TurnTransition;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void Game::Draw() {
    // Fase 1: desenha tudo numa textura de resolução fixa (cfg::SCREEN_WIDTH
    // x cfg::SCREEN_HEIGHT) — todo o resto do código de desenho continua
    // usando essas coordenadas fixas, sem se importar com a resolução real
    // da janela/tela.
    BeginTextureMode(virtualScreen);
    ClearBackground(Color{ 235, 214, 190, 255 });

    if (state == GameState::MainMenu) {
        DrawMainMenu();
#if !CANNON_DUEL_ANDROID_BUILD
        if (devMode) DrawDevPanel();
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawVirtualScreenScaled();
        EndDrawing();
        return;
    }

    if (state == GameState::About) {
        DrawAbout();
#if !CANNON_DUEL_ANDROID_BUILD
        if (devMode) DrawDevPanel();
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawVirtualScreenScaled();
        EndDrawing();
        return;
    }

    // ---- background (sprite gerado: dia com sol/nuvens, ou noite com estrelas/lua) ----
    Texture2D& bgTex = nightMode ? texBackgroundNight : texBackground;
    if (spritesReady && bgTex.id != 0) {
        DrawTexture(bgTex, 0, 0, WHITE);
    } else {
        DrawCircle(cfg::SCREEN_WIDTH - 140, 110, 60, Color{255, 221, 130, 255});
        DrawRectangle(0, cfg::SCREEN_HEIGHT - 460, cfg::SCREEN_WIDTH, 40, Color{225, 200, 175, 180});
    }

    // Screen shake (versão Plus): tudo dentro do "mundo do jogo" (terreno,
    // canhões, projétil, partículas, power-up, indicadores de mira) é
    // desenhado com um leve deslocamento de câmera que decai com o tempo.
    // O HUD e os overlays de UI ficam FORA dessa câmera, sempre estáveis.
    Camera2D shakeCam = { 0 };
    shakeCam.target = { 0.0f, 0.0f };
    shakeCam.offset = ComputeShakeOffset();
    shakeCam.rotation = 0.0f;
    shakeCam.zoom = 1.0f;
    BeginMode2D(shakeCam);

    terrain.Draw();
    player1.Draw(currentPlayer == 1 && state != GameState::RoundOver,
                 spritesReady ? &texCannonLeft : nullptr);
    player2.Draw(currentPlayer == 2 && state != GameState::RoundOver,
                 spritesReady ? &texCannonRight : nullptr);

    if (version == GameVersion::Plus) {
        DrawPowerup();
        DrawPowerupTooltip();
    }

    // Partículas (incluindo o rastro do projétil) desenhadas ANTES do
    // projétil em si — senão, como o rastro nasce exatamente na posição da
    // bala a cada frame, ele ficaria por cima e "pintaria" a bala com a cor
    // clara da fumaça, escondendo o preto original do sprite.
    particles.Draw();

    if (projectile.IsActive()) {
        Vector2 p = projectile.PositionPx();
        if (spritesReady && texProjectile.id != 0) {
            float r = cfg::PROJECTILE_RADIUS_PX;
            DrawTexturePro(texProjectile, {0, 0, (float)texProjectile.width, (float)texProjectile.height},
                           {p.x - r, p.y - r, r * 2, r * 2}, {0, 0}, 0.0f, WHITE);
        } else {
            DrawCircleV(p, cfg::PROJECTILE_RADIUS_PX, BLACK);
        }
    }

    // mecanismo de mira: linha oscilando (fase ângulo) ou barra de força (fase potência)
    if (state == GameState::Aiming && !(mode == GameMode::PvAI && currentPlayer == 2)) {
        Cannon& active = (currentPlayer == 1) ? player1 : player2;
        Vector2 base = { active.x, active.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };

        // power-up "trajetória prevista": desenha o arco balístico estimado
        // com o ângulo/força atuais (simulação simplificada, incluindo vento)
        if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
            Vector2 dir = active.AimDirection();
            float speed = cfg::MIN_POWER + active.power01 * (cfg::MAX_POWER - cfg::MIN_POWER);
            Vector2 simPos = active.MuzzlePosition();
            Vector2 simVel = { dir.x * speed * cfg::PPM, dir.y * speed * cfg::PPM }; // px/s
            float simDt = 0.05f;
            float windPxAccel = windForce * cfg::PPM;
            float gravPxAccel = cfg::GRAVITY_MPS2 * cfg::PPM;
            for (int i = 0; i < 90; ++i) {
                simVel.x += windPxAccel * simDt;
                simVel.y += gravPxAccel * simDt;
                simPos.x += simVel.x * simDt;
                simPos.y += simVel.y * simDt;
                if (simPos.y >= terrain.HeightAt(simPos.x) || simPos.x < 0 || simPos.x > cfg::SCREEN_WIDTH) break;
                if (i % 2 == 0) DrawCircleV(simPos, 2.5f, Fade(Color{60, 130, 220, 255}, 0.7f));
            }
        }

        if (aimPhase == AimPhase::Angle) {
            Vector2 dir = active.AimDirection();
            Vector2 tip = { base.x + dir.x * 100.0f, base.y + dir.y * 100.0f };
            DrawLineEx(base, tip, 3.0f, Fade(RED, 0.8f));
            DrawCircleV(tip, 4.0f, RED);
        } else {
            // barra de força acima do canhão: verde (fraco) -> vermelho (forte)
            float barW = 120.0f, barH = 16.0f;
            Vector2 barPos = { active.x - barW / 2, active.groundY - cfg::CANNON_BODY_RADIUS_PX - 60 };
            DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                          static_cast<int>(barW), static_cast<int>(barH), Color{30, 30, 30, 220});

            int segments = 24;
            for (int i = 0; i < segments; ++i) {
                float t = (i + 0.5f) / segments;
                Color c = { static_cast<unsigned char>(GREEN.r + (RED.r - GREEN.r) * t),
                            static_cast<unsigned char>(GREEN.g + (RED.g - GREEN.g) * t),
                            static_cast<unsigned char>(GREEN.b + (RED.b - GREEN.b) * t),
                            255 };
                float segX = barPos.x + t * barW;
                DrawRectangle(static_cast<int>(segX), static_cast<int>(barPos.y),
                              static_cast<int>(barW / segments) + 1, static_cast<int>(barH), c);
            }
            DrawRectangleLines(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                                static_cast<int>(barW), static_cast<int>(barH), BLACK);

            // marcador da posição atual (oscilando)
            float markerX = barPos.x + active.power01 * barW;
            DrawRectangle(static_cast<int>(markerX) - 2, static_cast<int>(barPos.y) - 4,
                          4, static_cast<int>(barH) + 8, WHITE);
        }
    }

    if (version == GameVersion::Plus) {
        DrawPowerupMessage();
    }

    EndMode2D();

    DrawHUD();

    if (state == GameState::RoundOver) {
        DrawRectangle(0, 0, cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, Fade(BLACK, 0.55f));
        const char* msg = ResolveRoundMessage();
        int fs = 48;
        int tw = MeasureText(msg, fs);
        DrawText(msg, cfg::SCREEN_WIDTH / 2 - tw / 2, cfg::SCREEN_HEIGHT / 2 - 60, fs, WHITE);
        const char* hint = T(TK::RoundOverHint, language);
        int hw = MeasureText(hint, 20);
        DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, cfg::SCREEN_HEIGHT / 2 + 10, 20, LIGHTGRAY);
    }

    DrawMenuButton();

    if (state == GameState::Aiming && !(mode == GameMode::PvAI && currentPlayer == 2)) {
        DrawResetAngleButton();
    }

#if !CANNON_DUEL_ANDROID_BUILD
    if (devMode) {
        DrawDevPanel();
    }
#endif

    if (showMenuConfirm) {
        DrawMenuConfirmDialog();
    }

    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    DrawVirtualScreenScaled();
    EndDrawing();
}

void Game::UpdateVersionSwitch(Vector2 mouse) {
    float cx = cfg::SCREEN_WIDTH / 2.0f;
    Rectangle full = { cx - 150, 240, 300, 50 };
    Rectangle leftHalf  = { full.x, full.y, full.width / 2, full.height };
    Rectangle rightHalf = { full.x + full.width / 2, full.y, full.width / 2, full.height };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, leftHalf)) version = GameVersion::Classic;
        else if (CheckCollisionPointRec(mouse, rightHalf)) version = GameVersion::Plus;
    }
}

void Game::DrawVersionSwitch(Vector2 mouse) {
    float cx = cfg::SCREEN_WIDTH / 2.0f;
    Rectangle full = { cx - 150, 240, 300, 50 };
    Rectangle leftHalf  = { full.x, full.y, full.width / 2, full.height };
    Rectangle rightHalf = { full.x + full.width / 2, full.y, full.width / 2, full.height };

    DrawRectangleRec(full, Color{60, 45, 30, 255});

    bool classicSel = (version == GameVersion::Classic);
    DrawRectangleRec(leftHalf, classicSel ? Color{230, 180, 90, 255} : Color{90, 75, 55, 255});
    DrawRectangleRec(rightHalf, !classicSel ? Color{230, 130, 60, 255} : Color{90, 75, 55, 255});
    DrawRectangleLinesEx(full, 2, Color{30, 22, 12, 255});
    DrawLineEx({cx, full.y}, {cx, full.y + full.height}, 2, Color{30, 22, 12, 255});

    int fs = 20;
    const char* lbl1 = T(TK::ClassicLabel, language);
    const char* lbl2 = T(TK::PlusLabel, language);
    int w1 = MeasureText(lbl1, fs);
    int w2 = MeasureText(lbl2, fs);
    DrawText(lbl1, static_cast<int>(leftHalf.x + leftHalf.width / 2 - w1 / 2),
             static_cast<int>(leftHalf.y + leftHalf.height / 2 - fs / 2), fs,
             classicSel ? Color{40, 25, 10, 255} : Color{230, 220, 210, 255});
    DrawText(lbl2, static_cast<int>(rightHalf.x + rightHalf.width / 2 - w2 / 2),
             static_cast<int>(rightHalf.y + rightHalf.height / 2 - fs / 2), fs,
             !classicSel ? Color{40, 15, 0, 255} : Color{230, 220, 210, 255});

    (void)mouse;
}

// ---------------------------------------------------------------------------
// Bandeiras de idioma (canto superior direito do menu principal)
// Desenhadas de forma procedural (retângulos/formas simples) — sem depender
// de nenhum arquivo de imagem, sempre nítidas em qualquer resolução.
// ---------------------------------------------------------------------------
namespace {
Rectangle BrazilFlagRect() { return { cfg::SCREEN_WIDTH - 180.0f, 20.0f, 64.0f, 44.0f }; }
Rectangle UsaFlagRect()    { return { cfg::SCREEN_WIDTH - 100.0f, 20.0f, 64.0f, 44.0f }; }

void DrawBrazilFlag(Rectangle r) {
    DrawRectangleRec(r, Color{0, 155, 58, 255}); // verde
    Vector2 c = { r.x + r.width / 2, r.y + r.height / 2 };
    Vector2 diamond[4] = {
        { c.x, r.y + 5 }, { r.x + r.width - 6, c.y }, { c.x, r.y + r.height - 5 }, { r.x + 6, c.y }
    };
    DrawTriangle(diamond[0], diamond[3], diamond[1], Color{255, 223, 0, 255});
    DrawTriangle(diamond[1], diamond[3], diamond[2], Color{255, 223, 0, 255});
    DrawCircleV(c, 9.0f, Color{0, 39, 118, 255}); // "globo" central simplificado
}

void DrawUsaFlag(Rectangle r) {
    DrawRectangleRec(r, Color{178, 34, 52, 255}); // fundo vermelho (listras ímpares)
    // Usa aritmética em ponto flutuante (não inteiro) para as listras,
    // garantindo que a soma exata das 7 faixas não ultrapasse a altura da
    // bandeira — arredondamento com "+1" antes fazia a última faixa branca
    // vazar para fora do retângulo/realce de seleção.
    float stripeH = r.height / 7.0f;
    for (int i = 0; i < 7; i += 2) {
        Rectangle stripe = { r.x, r.y + i * stripeH, r.width, stripeH };
        DrawRectangleRec(stripe, WHITE);
    }
    Rectangle canton = { r.x, r.y, r.width * 0.42f, r.height * 0.55f };
    DrawRectangleRec(canton, Color{60, 59, 110, 255});
    // pontinhos brancos representando as estrelas, simplificado
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sx = canton.x + canton.width * (0.2f + col * 0.3f);
            float sy = canton.y + canton.height * (0.22f + row * 0.32f);
            DrawCircleV({sx, sy}, 1.6f, WHITE);
        }
    }
}
} // namespace

void Game::UpdateLanguageFlags(Vector2 mouse) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, BrazilFlagRect())) language = Lang::PT_BR;
        else if (CheckCollisionPointRec(mouse, UsaFlagRect())) language = Lang::EN;
    }
}

void Game::DrawLanguageFlags(Vector2 mouse) {
    Rectangle br = BrazilFlagRect();
    Rectangle us = UsaFlagRect();

    bool brSel = (language == Lang::PT_BR);
    bool usSel = (language == Lang::EN);

    DrawBrazilFlag(br);
    DrawRectangleLinesEx(br, brSel ? 3.0f : 1.5f, brSel ? Color{255, 210, 60, 255} : Color{30, 22, 12, 200});

    DrawUsaFlag(us);
    DrawRectangleLinesEx(us, usSel ? 3.0f : 1.5f, usSel ? Color{255, 210, 60, 255} : Color{30, 22, 12, 200});

    // leve destaque de hover
    if (CheckCollisionPointRec(mouse, br) && !brSel) DrawRectangleLinesEx(br, 2.0f, Color{255, 255, 255, 180});
    if (CheckCollisionPointRec(mouse, us) && !usSel) DrawRectangleLinesEx(us, 2.0f, Color{255, 255, 255, 180});
}

void Game::DrawMainMenu() {
    const char* title = T(TK::Title, language);
    int fs = 64;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 160, fs, Color{40, 30, 20, 255});

    Vector2 m = GetVirtualMouse();
    DrawVersionSwitch(m);
    DrawLanguageFlags(m);

    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 330, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 406, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 482, 280, 56 };

    auto drawButton = [&](Rectangle r, const char* label) {
        bool hover = CheckCollisionPointRec(m, r);
        DrawRectangleRec(r, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleLinesEx(r, 2, Color{60, 40, 20, 255});
        int fs2 = 22;
        int tw2 = MeasureText(label, fs2);
        DrawText(label, static_cast<int>(r.x + r.width / 2 - tw2 / 2),
                 static_cast<int>(r.y + r.height / 2 - fs2 / 2), fs2, Color{40, 25, 10, 255});
    };

    drawButton(btn1P, T(TK::OnePlayer, language));
    drawButton(btn2P, T(TK::TwoPlayers, language));
    drawButton(btnAbout, T(TK::AboutButton, language));

    const char* hint = (version == GameVersion::Plus) ? T(TK::HintPlus, language) : T(TK::HintClassic, language);
    int hw = MeasureText(hint, 18);
    DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, 562, 18, Color{70, 55, 40, 255});
}

void Game::DrawAbout() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = T(TK::AboutTitle, language);
    int fs = 44;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 80, fs, Color{40, 30, 20, 255});

    int lineCount = 0;
    const char** lines = TextSplit(T(TK::AboutBody, language), '\n', &lineCount);

    int y = 160;
    int fs2 = 20;
    for (int i = 0; i < lineCount; ++i) {
        int lw = MeasureText(lines[i], fs2);
        DrawText(lines[i], cfg::SCREEN_WIDTH / 2 - lw / 2, y, fs2, Color{60, 45, 30, 255});
        y += 30;
    }

    const char* credit = T(TK::AboutCredit, language);
    int fsC = 26;
    int cw = MeasureText(credit, fsC);
    DrawText(credit, cfg::SCREEN_WIDTH / 2 - cw / 2, y + 20, fsC, Color{200, 120, 40, 255});

    Vector2 m = GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 100.0f, 200, 52 };
    bool hover = CheckCollisionPointRec(m, backBtn);
    DrawRectangleRec(backBtn, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLabel = T(TK::AboutBack, language);
    int blw = MeasureText(backLabel, 22);
    DrawText(backLabel, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}

void Game::UpdateMenuConfirmDialog() {
    Vector2 m = GetVirtualMouse();
    float cx = cfg::SCREEN_WIDTH / 2.0f, cy = cfg::SCREEN_HEIGHT / 2.0f;
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, yesBtn)) {
            showMenuConfirm = false;
            projectile.Destroy();
            if (audioReady) StopMusicStream(music);
            state = GameState::MainMenu;
        } else if (CheckCollisionPointRec(m, noBtn)) {
            showMenuConfirm = false;
        }
    }
}

void Game::DrawMenuConfirmDialog() const {
    DrawRectangle(0, 0, cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, Fade(BLACK, 0.6f));

    float cx = cfg::SCREEN_WIDTH / 2.0f, cy = cfg::SCREEN_HEIGHT / 2.0f;
    Rectangle panel = { cx - 200, cy - 90, 400, 200 };
    DrawRectangleRec(panel, Color{245, 240, 230, 255});
    DrawRectangleLinesEx(panel, 3, Color{30, 25, 20, 255});

    const char* msg = T(TK::ConfirmTitle, language);
    int fs = 24;
    int tw = MeasureText(msg, fs);
    DrawText(msg, static_cast<int>(cx - tw / 2), static_cast<int>(cy - 60), fs, Color{30, 25, 20, 255});

    const char* sub = T(TK::ConfirmSub, language);
    int fs2 = 16;
    int tw2 = MeasureText(sub, fs2);
    DrawText(sub, static_cast<int>(cx - tw2 / 2), static_cast<int>(cy - 28), fs2, Color{90, 80, 70, 255});

    Vector2 m = GetVirtualMouse();
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    bool hoverYes = CheckCollisionPointRec(m, yesBtn);
    bool hoverNo  = CheckCollisionPointRec(m, noBtn);

    DrawRectangleRec(yesBtn, hoverYes ? Color{220, 90, 80, 255} : Color{200, 70, 60, 255});
    DrawRectangleLinesEx(yesBtn, 2, Color{30, 25, 20, 255});
    const char* yesLabel = T(TK::ConfirmYes, language);
    int ytw = MeasureText(yesLabel, 18);
    DrawText(yesLabel, static_cast<int>(yesBtn.x + yesBtn.width / 2 - ytw / 2),
             static_cast<int>(yesBtn.y + 15), 18, WHITE);

    DrawRectangleRec(noBtn, hoverNo ? Color{140, 190, 130, 255} : Color{120, 170, 110, 255});
    DrawRectangleLinesEx(noBtn, 2, Color{30, 25, 20, 255});
    const char* noLabel = T(TK::ConfirmNo, language);
    int ntw = MeasureText(noLabel, 18);
    DrawText(noLabel, static_cast<int>(noBtn.x + noBtn.width / 2 - ntw / 2),
             static_cast<int>(noBtn.y + 15), 18, WHITE);
}

void Game::DrawMenuButton() const {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 16.0f, 134.0f, 40.0f };
    Vector2 m = GetVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hover ? Color{235, 235, 235, 235} : Color{20, 20, 20, 170});
    DrawRectangleLinesEx(r, 2, hover ? Color{20, 20, 20, 255} : Color{235, 235, 235, 200});

    const char* label = T(TK::MenuButton, language);
    int fs = 20;
    int tw = MeasureText(label, fs);
    Color textColor = hover ? Color{20, 20, 20, 255} : Color{240, 240, 240, 255};
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleMenuButtonClick() {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 16.0f, 134.0f, 40.0f };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetVirtualMouse(), r)) {
        return true;
    }
    return false;
}

void Game::DrawResetAngleButton() const {
    // Posicionado logo abaixo do botão de menu — botão in-game, disponível
    // tanto no PC (via mouse) quanto no Android (via toque), equivalente à
    // tecla B do atalho de teclado exclusivo de PC.
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 66.0f, 134.0f, 40.0f };
    Vector2 m = GetVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hover ? Color{235, 235, 235, 235} : Color{20, 20, 20, 170});
    DrawRectangleLinesEx(r, 2, hover ? Color{20, 20, 20, 255} : Color{235, 235, 235, 200});

    const char* label = T(TK::ResetAngleButton, language);
    int fs = 14;
    int tw = MeasureText(label, fs);
    Color textColor = hover ? Color{20, 20, 20, 255} : Color{240, 240, 240, 255};
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleResetAngleButtonClick() {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 66.0f, 134.0f, 40.0f };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetVirtualMouse(), r)) {
        return true;
    }
    return false;
}

void Game::DrawWindIndicator() const {
    int cx = cfg::SCREEN_WIDTH / 2;
    int cy = 40;
    DrawText(T(TK::WindLabel, language), cx - 30, cy - 22, 16, HudTextColor());

    float ratio = windForce / cfg::WIND_MAX_ACCEL; // -1..1
    int arrowLen = static_cast<int>(std::fabs(ratio) * 60.0f) + 10;
    Color c = (ratio >= 0) ? Color{200, 60, 60, 255} : Color{60, 120, 200, 255};

    if (ratio >= 0) {
        DrawTriangle({(float)(cx + arrowLen), (float)cy}, {(float)cx, (float)(cy - 8)}, {(float)cx, (float)(cy + 8)}, c);
        DrawLineEx({(float)cx, (float)cy}, {(float)(cx + arrowLen), (float)cy}, 4, c);
    } else {
        DrawTriangle({(float)(cx - arrowLen), (float)cy}, {(float)cx, (float)(cy + 8)}, {(float)cx, (float)(cy - 8)}, c);
        DrawLineEx({(float)cx, (float)cy}, {(float)(cx - arrowLen), (float)cy}, 4, c);
    }
}

void Game::DrawHUD() {
    DrawWindIndicator();

    const char* turnLabel = (mode == GameMode::PvAI && currentPlayer == 2)
        ? T(TK::TurnAI, language)
        : (currentPlayer == 1 ? T(TK::TurnPlayer1, language) : T(TK::TurnPlayer2, language));
    DrawText(turnLabel, 20, 20, 22, HudTextColor());

    // ângulo/potência do jogador ativo (útil para jogar só com mouse)
    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    const char* angleForceFmt = (language == Lang::PT_BR) ? "Angulo: %.0f  Forca: %.0f%%" : "Angle: %.0f  Power: %.0f%%";
    std::string info = TextFormat(angleForceFmt, active.angleDeg, active.power01 * 100.0f);
    DrawText(info.c_str(), 20, 48, 18, HudTextColorDim());
}
