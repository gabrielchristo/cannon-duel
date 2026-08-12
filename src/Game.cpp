#include "Game.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>

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
    // apenas espera clique nos botões (tratado no Draw via retângulos, checado aqui)
    Vector2 m = GetMousePosition();
    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 320, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 396, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 472, 280, 56 };

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

void Game::UpdateAiming() {
    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    Cannon& other  = (currentPlayer == 1) ? player2 : player1;

    bool isAITurn = (mode == GameMode::PvAI && currentPlayer == 2);

    if (isAITurn) {
        // IA "pensa" e atira quase imediatamente (poderia adicionar delay/timer)
        ai.ComputeShot(active, other, windForce);

        Vector2 muzzle = active.MuzzlePosition();
        Vector2 dir = active.AimDirection();
        projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
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
            Vector2 dir = active.AimDirection();
            projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
            if (audioReady) PlaySound(sndFire);
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            state = GameState::ProjectileFlying;
        }
    }
}

void Game::UpdateProjectileFlight(float dt) {
    projectile.ApplyWind(windForce);
    physics.Step(dt);

    if (!projectile.IsActive()) return;

    Vector2 pos = projectile.PositionPx();
    particles.EmitTrail(pos, projectile.VelocityPx());

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

    particles.EmitExplosion(impactPos, 50);
    terrain.Explode(impactPos.x, impactPos.y, cfg::CRATER_RADIUS_PX);

    // dano em área para os dois canhões, ponderado pela distância
    Cannon* cannons[2] = { &player1, &player2 };
    for (Cannon* c : cannons) {
        Vector2 base = { c->x, c->groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
        float dist = std::sqrt(std::pow(impactPos.x - base.x, 2) + std::pow(impactPos.y - base.y, 2));
        if (dist <= cfg::EXPLOSION_RADIUS_PX) {
            float falloff = 1.0f - (dist / cfg::EXPLOSION_RADIUS_PX);
            float dmg = cfg::EXPLOSION_DAMAGE_MAX * falloff;
            if (hitCannon && c == hitTarget) dmg = cfg::EXPLOSION_DAMAGE_MAX; // impacto direto
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
        EndDrawing();
        return;
    }

    if (state == GameState::About) {
        DrawAbout();
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

    terrain.Draw();
    player1.Draw(currentPlayer == 1 && state != GameState::RoundOver,
                 spritesReady ? &texCannonLeft : nullptr);
    player2.Draw(currentPlayer == 2 && state != GameState::RoundOver,
                 spritesReady ? &texCannonRight : nullptr);

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

    if (showMenuConfirm) {
        DrawMenuConfirmDialog();
    }

    EndDrawing();
}

void Game::DrawMainMenu() {
    const char* title = "CANNON DUEL";
    int fs = 64;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 160, fs, Color{40, 30, 20, 255});

    Vector2 m = GetMousePosition();
    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 320, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 396, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 472, 280, 56 };

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

    const char* hint = "Clique para travar o angulo e a forca (botao direito volta ao angulo)";
    int hw = MeasureText(hint, 18);
    DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, 556, 18, Color{70, 55, 40, 255});
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
