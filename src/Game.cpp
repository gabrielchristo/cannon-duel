#include "Game.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>

namespace {
float RandF(float lo, float hi) {
    return lo + static_cast<float>(rand()) / RAND_MAX * (hi - lo);
}
} // namespace

Game::Game() {
    InitWindow(cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, "Cannon Duel");
    SetTargetFPS(cfg::TARGET_FPS);

    InitAudioDevice();
    audioReady = IsAudioDeviceReady();
    if (audioReady) {
        sndFire      = LoadSound("assets/sounds/fire.wav");
        sndExplosion = LoadSound("assets/sounds/explosion.wav");
    }

    texCannonLeft   = LoadTexture("assets/sprites/cannon_left.png");
    texCannonRight  = LoadTexture("assets/sprites/cannon_right.png");
    texBackground   = LoadTexture("assets/sprites/background.png");
    texBackgroundNight = LoadTexture("assets/sprites/background_night.png");
    texTerrainTile  = LoadTexture("assets/sprites/terrain_tile.png");
    texProjectile   = LoadTexture("assets/sprites/projectile.png");
    spritesReady = (texCannonLeft.id != 0 && texCannonRight.id != 0);

    srand(static_cast<unsigned int>(time(nullptr)));
}

Game::~Game() {
    if (audioReady) {
        UnloadSound(sndFire);
        UnloadSound(sndExplosion);
        CloseAudioDevice();
    }
    UnloadTexture(texCannonLeft);
    UnloadTexture(texCannonRight);
    UnloadTexture(texBackground);
    UnloadTexture(texBackgroundNight);
    UnloadTexture(texTerrainTile);
    UnloadTexture(texProjectile);
    CloseWindow();
}

void Game::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
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
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void Game::Update(float dt) {
    // Painel de desenvolvedor oculto: F9 alterna a visibilidade. Não é
    // exposto em nenhum menu/UI normal — é só um atalho de teclado para
    // testes internos.
    if (IsKeyPressed(KEY_F9)) {
        devMode = !devMode;
    }
    if (devMode && UpdateDevPanel()) {
        return;
    }

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
                state = GameState::MainMenu;
            }
            break;
    }
}

void Game::UpdateMainMenu() {
    Vector2 m = GetMousePosition();
    UpdateVersionSwitch(m);

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
    Vector2 m = GetMousePosition();
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
    Vector2 mouse = GetMousePosition();

    for (const auto& pu : activePowerups) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        float dist = std::sqrt(std::pow(mouse.x - center.x, 2) + std::pow(mouse.y - center.y, 2));
        if (dist > cfg::POWERUP_RADIUS_PX + 10.0f) continue;

        const char* desc = PowerupDescription(pu.type);
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
    ShowPowerupMessage(PowerupDescription(type), base);
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
// ---------------------------------------------------------------------------
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
    Vector2 m = GetMousePosition();
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

    Vector2 m = GetMousePosition();
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

void Game::UpdateAiming() {
    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    Cannon& other  = (currentPlayer == 1) ? player2 : player1;

    bool isAITurn = (mode == GameMode::PvAI && currentPlayer == 2);

    if (isAITurn) {
        // IA "pensa" e atira quase imediatamente (poderia adicionar delay/timer)
        ai.ComputeShot(active, other, windForce);

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

    if (aimPhase == AimPhase::Angle) {
        // 0° a 90° na direção do oponente, oscilando continuamente (onda triangular).
        float period = cfg::ANGLE_OSC_PERIOD_SEC;
        float t = fmodf(aimOscTimer, period) / period; // 0..1
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
        float angle = tri * 90.0f;
        active.SetAim(angle, active.power01);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            aimPhase = AimPhase::Power;
            aimOscTimer = 0.0f;
        }
    } else { // AimPhase::Power
        float period = cfg::POWER_OSC_PERIOD_SEC;
        float t = fmodf(aimOscTimer, period) / period; // 0..1
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
        active.SetAim(active.angleDeg, tri);

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            // volta para a seleção de ângulo, começando a oscilação do zero
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            return;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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

    // colisão com terreno (heightmap)
    if (terrain.IsPointInside(pos.x, pos.y)) {
        ResolveImpact(pos, false, nullptr);
        return;
    }

    // colisão com canhão adversário (círculo simples)
    Cannon& target = (currentPlayer == 1) ? player2 : player1;
    Vector2 targetBase = { target.x, target.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
    float distToTarget = std::sqrt(std::pow(pos.x - targetBase.x, 2) + std::pow(pos.y - targetBase.y, 2));
    if (distToTarget <= cfg::CANNON_BODY_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX) {
        ResolveImpact(pos, true, &target);
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

        roundMessage = (p1Buried && p2Buried) ? "EMPATE! (ambos soterrados)"
                       : (p1Buried ? "JOGADOR 2 VENCEU! (canhao 1 soterrado)"
                                   : "JOGADOR 1 VENCEU! (canhao 2 soterrado)");
        state = GameState::RoundOver;
        stateTimer = 1.0f;
        return;
    }

    CheckRoundEnd();
}

void Game::CheckRoundEnd() {
    if (!player1.IsAlive() || !player2.IsAlive()) {
        roundMessage = (!player1.IsAlive() && !player2.IsAlive()) ? "EMPATE!"
                       : (!player1.IsAlive() ? "JOGADOR 2 VENCEU!" : "JOGADOR 1 VENCEU!");
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
    BeginDrawing();
    ClearBackground(Color{ 235, 214, 190, 255 });

    if (state == GameState::MainMenu) {
        DrawMainMenu();
        if (devMode) DrawDevPanel();
        EndDrawing();
        return;
    }

    if (state == GameState::About) {
        DrawAbout();
        if (devMode) DrawDevPanel();
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
        int fs = 48;
        int tw = MeasureText(roundMessage, fs);
        DrawText(roundMessage, cfg::SCREEN_WIDTH / 2 - tw / 2, cfg::SCREEN_HEIGHT / 2 - 60, fs, WHITE);
        const char* hint = "clique para voltar ao menu";
        int hw = MeasureText(hint, 20);
        DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, cfg::SCREEN_HEIGHT / 2 + 10, 20, LIGHTGRAY);
    }

    DrawMenuButton();

    if (devMode) {
        DrawDevPanel();
    }

    if (showMenuConfirm) {
        DrawMenuConfirmDialog();
    }

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
    const char* lbl1 = "CLASSIC";
    const char* lbl2 = "PLUS";
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

void Game::DrawMainMenu() {
    const char* title = "CANNON DUEL";
    int fs = 64;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 160, fs, Color{40, 30, 20, 255});

    Vector2 m = GetMousePosition();
    DrawVersionSwitch(m);

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

    drawButton(btn1P, "1 JOGADOR (vs IA)");
    drawButton(btn2P, "2 JOGADORES");
    drawButton(btnAbout, "SOBRE");

    const char* hint = (version == GameVersion::Plus)
        ? "PLUS: power-ups aparecem no mapa a cada 2 rodadas!"
        : "Clique para travar o angulo e a forca (botao direito volta ao angulo)";
    int hw = MeasureText(hint, 18);
    DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, 562, 18, Color{70, 55, 40, 255});
}

void Game::DrawAbout() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = "SOBRE O JOGO";
    int fs = 44;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 80, fs, Color{40, 30, 20, 255});

    const char* paragraphs[] = {
        "Cannon Duel e uma releitura moderna, feita do zero, do jogo",
        "\"Canhao\", lancado originalmente pela TecToy/Devworks para o",
        "Mega Drive em 2005. O objetivo aqui foi recriar a essencia da",
        "jogabilidade classica de artilharia por turnos - mira, forca,",
        "vento e destruicao de terreno - com fisica realista (Box2D),",
        "renderizacao propria (raylib) e uma identidade visual nova.",
        "",
        "Nenhum grafico, som ou dado original da ROM foi utilizado:",
        "toda a arte deste jogo foi criada do zero.",
    };

    int y = 160;
    int fs2 = 20;
    for (const char* line : paragraphs) {
        int lw = MeasureText(line, fs2);
        DrawText(line, cfg::SCREEN_WIDTH / 2 - lw / 2, y, fs2, Color{60, 45, 30, 255});
        y += 30;
    }

    const char* credit = "Desenvolvido por Gabriel Christo";
    int fsC = 26;
    int cw = MeasureText(credit, fsC);
    DrawText(credit, cfg::SCREEN_WIDTH / 2 - cw / 2, y + 20, fsC, Color{200, 120, 40, 255});

    Vector2 m = GetMousePosition();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 100.0f, 200, 52 };
    bool hover = CheckCollisionPointRec(m, backBtn);
    DrawRectangleRec(backBtn, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLabel = "VOLTAR";
    int blw = MeasureText(backLabel, 22);
    DrawText(backLabel, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}

void Game::UpdateMenuConfirmDialog() {
    Vector2 m = GetMousePosition();
    float cx = cfg::SCREEN_WIDTH / 2.0f, cy = cfg::SCREEN_HEIGHT / 2.0f;
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, yesBtn)) {
            showMenuConfirm = false;
            projectile.Destroy();
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

    const char* msg = "Voltar ao menu inicial?";
    int fs = 24;
    int tw = MeasureText(msg, fs);
    DrawText(msg, static_cast<int>(cx - tw / 2), static_cast<int>(cy - 60), fs, Color{30, 25, 20, 255});

    const char* sub = "A partida atual sera perdida.";
    int fs2 = 16;
    int tw2 = MeasureText(sub, fs2);
    DrawText(sub, static_cast<int>(cx - tw2 / 2), static_cast<int>(cy - 28), fs2, Color{90, 80, 70, 255});

    Vector2 m = GetMousePosition();
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    bool hoverYes = CheckCollisionPointRec(m, yesBtn);
    bool hoverNo  = CheckCollisionPointRec(m, noBtn);

    DrawRectangleRec(yesBtn, hoverYes ? Color{220, 90, 80, 255} : Color{200, 70, 60, 255});
    DrawRectangleLinesEx(yesBtn, 2, Color{30, 25, 20, 255});
    const char* yesLabel = "SIM, SAIR";
    int ytw = MeasureText(yesLabel, 18);
    DrawText(yesLabel, static_cast<int>(yesBtn.x + yesBtn.width / 2 - ytw / 2),
             static_cast<int>(yesBtn.y + 15), 18, WHITE);

    DrawRectangleRec(noBtn, hoverNo ? Color{140, 190, 130, 255} : Color{120, 170, 110, 255});
    DrawRectangleLinesEx(noBtn, 2, Color{30, 25, 20, 255});
    const char* noLabel = "CONTINUAR";
    int ntw = MeasureText(noLabel, 18);
    DrawText(noLabel, static_cast<int>(noBtn.x + noBtn.width / 2 - ntw / 2),
             static_cast<int>(noBtn.y + 15), 18, WHITE);
}

void Game::DrawMenuButton() const {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 16.0f, 134.0f, 40.0f };
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hover ? Color{235, 235, 235, 235} : Color{20, 20, 20, 170});
    DrawRectangleLinesEx(r, 2, hover ? Color{20, 20, 20, 255} : Color{235, 235, 235, 200});

    const char* label = "MENU";
    int fs = 20;
    int tw = MeasureText(label, fs);
    Color textColor = hover ? Color{20, 20, 20, 255} : Color{240, 240, 240, 255};
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleMenuButtonClick() {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 16.0f, 134.0f, 40.0f };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r)) {
        return true;
    }
    return false;
}

void Game::DrawWindIndicator() const {
    int cx = cfg::SCREEN_WIDTH / 2;
    int cy = 40;
    DrawText("VENTO", cx - 30, cy - 22, 16, HudTextColor());

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

    std::string turnLabel = "Vez do Jogador " + std::to_string(currentPlayer);
    if (mode == GameMode::PvAI && currentPlayer == 2) turnLabel = "Vez da IA";
    DrawText(turnLabel.c_str(), 20, 20, 22, HudTextColor());

    // ângulo/potência do jogador ativo (útil para jogar só com mouse)
    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    std::string info = TextFormat("Angulo: %.0f  Forca: %.0f%%", active.angleDeg, active.power01 * 100.0f);
    DrawText(info.c_str(), 20, 48, 18, HudTextColorDim());
}
