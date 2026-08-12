#include "Game.h"
#include "AssetPath.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "VirtualScreen.h"
#include "net/NetWorker.h"
#include "net/SupabaseClient.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

bool Game::IsLocalHumanTurn() const {
    if (state != GameState::Aiming) return false;
    if (mode == GameMode::PvAI && currentPlayer == 2) return false;
    if (mode == GameMode::Online) {
        if (!netMatch.IsMyTurn()) return false;
        if (currentPlayer != netMatch.MyPlayerNumber()) return false;
    }
    return true;
}

void Game::UpdateAiming() {
    // Multiplayer online: só controlo o MEU canhão quando o servidor diz
    // que é minha vez — nunca o canhão indicado por currentPlayer sozinho.
    if (mode == GameMode::Online && !netMatch.IsMyTurn()) {
        return;
    }

    int activePlayer = currentPlayer;
    if (mode == GameMode::Online) {
        activePlayer = netMatch.MyPlayerNumber();
    }
    Cannon& active = (activePlayer == 1) ? player1 : player2;
    Cannon& other  = (activePlayer == 1) ? player2 : player1;

    bool isAITurn = (mode == GameMode::PvAI && currentPlayer == 2);

    if (isAITurn) {
        // IA "pensa" e atira quase imediatamente (poderia adicionar delay/timer)
        float targetX = other.x;
        float targetY = other.groundY;

        // Versão Plus: a IA às vezes prefere mirar num power-up no mapa em
        // vez de atacar o adversário diretamente — com prioridade maior
        // quando está com pouca vida (cura/escudo) e uma chance geral menor
        // pros demais casos, pra não ficar sempre ignorando o adversário.
        if (version == GameVersion::Plus && !powerups.Active().empty()) {
            float healthRatio = active.health / cfg::CANNON_MAX_HEALTH;
            int chosenIdx = -1;
            float bestPriority = 0.0f;

            for (size_t i = 0; i < powerups.Active().size(); ++i) {
                const Powerup& pu = powerups.Active()[i];
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
                targetX = powerups.Active()[static_cast<size_t>(chosenIdx)].x;
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
        guidedDiving = false;
        // Trajetória prevista é consumida no exato momento do disparo — é
        // aqui que o jogador de fato "usou" a rodada com o preview visível.
        if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
            active.trajectoryPreviewTurnsLeft--;
        }
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
            guidedDiving = false;
            if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
                active.trajectoryPreviewTurnsLeft--;
            }
            if (audioReady) PlaySound(sndFire);
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            state = GameState::ProjectileFlying;
            powerups.ShotPickedType() = -1;
            powerups.ShotPickedX() = 0.0f;
            if (mode == GameMode::Online && netMatch.InMatch()) {
                Vector2 muzz = active.MuzzlePosition();
                netMatch.PublishShotFired(active.angleDeg, active.power01, windForce, muzz.x, muzz.y);
            }
        }
    }

    if (mode == GameMode::Online && netMatch.InMatch()) {
        const char* phaseStr = (aimPhase == AimPhase::Angle) ? "angle" : "power";
        netMatch.PublishLiveAim(GetFrameTime(), active.angleDeg, active.power01, phaseStr);
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
        //
        // IMPORTANTE: uma vez que entra em modo "mergulho", NUNCA mais volta
        // pro modo "subir" — sem essa trava (guidedDiving), se a distância
        // horizontal oscilasse pra frente e pra trás bem em cima do limiar
        // (por causa do vento, por exemplo), o alvo da mira alternava entre
        // "céu" e "canhão" a cada frame, fazendo o projétil guinar
        // erraticamente pra cima e pra baixo sem parar — exatamente o
        // comportamento instável relatado.
        Vector2 projPos = projectile.PositionPx();
        if (!guidedDiving) {
            float horizDist = std::fabs(projPos.x - opponent.x);
            if (horizDist <= cfg::POWERUP_GUIDED_DIVE_DIST_PX) {
                guidedDiving = true;
            }
        }
        Vector2 targetPos = guidedDiving
            ? Vector2{ opponent.x, opponent.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f }
            : Vector2{ opponent.x, cfg::POWERUP_GUIDED_APEX_Y_PX };
        projectile.ApplyGuidance(targetPos, cfg::POWERUP_GUIDED_TURN_RATE_DEG, dt);
    }

    physics.Step(dt);

    if (!projectile.IsActive()) return;

    Vector2 pos = projectile.PositionPx();

    if (mode == GameMode::Online && netMatch.InMatch()) {
        netMatch.PublishProjectileSample(dt, pos.x, pos.y);
    }

    Color trailColor = (version == GameVersion::Plus && shooter.pendingDoubleDamage)
        ? Color{255, 130, 40, 255}  // rastro em chamas (dano em dobro)
        : Color{235, 230, 215, 255};
    particles.EmitTrail(pos, projectile.VelocityPx(), trailColor);

    if (version == GameVersion::Plus) {
        powerups.CheckProjectileCollision(prevProjectilePos, pos, currentPlayer,
                                          player1, player2, terrain, language,
                                          [this](int type, float x) {
                                              if (mode == GameMode::Online && netMatch.InMatch()) {
                                                  netMatch.PublishPowerupPicked(type, x);
                                              }
                                          });
    }
    prevProjectilePos = pos;

    // fora da tela (nunca deveria bater em nada) -> encerra o turno
    if (pos.x < -50 || pos.x > cfg::SCREEN_WIDTH + 50 || pos.y > cfg::SCREEN_HEIGHT + 200) {
        // ResolveImpact também envia o turno online — EndTurn() sozinho
        // deixaria o adversário esperando para sempre.
        ResolveImpact(pos, false, nullptr);
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
    bool wasGuided = false;

    if (version == GameVersion::Plus) {
        if (shooter.pendingDoubleDamage) { damageMult *= cfg::POWERUP_DOUBLE_DAMAGE_MULT; }
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

        // Mesma promoção "atrasada" pra trajetória prevista — só começa a
        // contar a partir da próxima rodada do jogador.
        if (shooter.queuedTrajectoryPreviewTurns > 0) {
            shooter.trajectoryPreviewTurnsLeft = shooter.queuedTrajectoryPreviewTurns;
            shooter.queuedTrajectoryPreviewTurns = 0;
        }
    }

    float craterRadius = cfg::CRATER_RADIUS_PX * radiusMult;
    float explosionRadius = cfg::EXPLOSION_RADIUS_PX * radiusMult;
    int particleCount = 50;

    particles.EmitExplosion(impactPos, particleCount);
    terrain.Explode(impactPos.x, impactPos.y, craterRadius);

    if (version == GameVersion::Plus) {
        effects.TriggerShake(hitCannon ? cfg::SHAKE_MAGNITUDE_DIRECT_PX : cfg::SHAKE_MAGNITUDE_TERRAIN_PX,
                             hitCannon ? cfg::SHAKE_DURATION_DIRECT_SEC : cfg::SHAKE_DURATION_TERRAIN_SEC);
    }
    (void)wasGuided;

    // dano em área para os dois canhões, ponderado pela distância
    float dmgAppliedP1 = 0.0f, dmgAppliedP2 = 0.0f;
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
            if (c == &player1) dmgAppliedP1 = dmg; else dmgAppliedP2 = dmg;
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
    } else {
        CheckRoundEnd(); // pode terminar a partida (RoundOver) ou chamar EndTurn()
    }

    // Multiplayer online: eu (o atirador) mando o RESULTADO autoritativo
    // (impacto, cratera, dano). O adversário só aplica esse pacote — nunca
    // recalcula Box2D. Assim terreno/dano ficam idênticos nos dois clientes.
    if (mode == GameMode::Online) {
        // Captura o vento do disparo ANTES de qualquer troca de turno.
        const float windAtShot = windForce;
        bool matchOver = (state == GameState::RoundOver);
        int winnerPlayer = 0;
        if (matchOver) {
            switch (roundOutcome) {
                case RoundOutcome::P1Wins: case RoundOutcome::P1WinsBuried: winnerPlayer = 1; break;
                case RoundOutcome::P2Wins: case RoundOutcome::P2WinsBuried: winnerPlayer = 2; break;
                default: winnerPlayer = 0; break;
            }
        }

        int nextTurnIndex = netMatch.TurnsCompleted() + 1;
        float nextWind = matchOver ? windForce : SeededWind(nextTurnIndex);

        netMatch.PublishShotEnded(impactPos.x, impactPos.y);
        netMatch.SubmitMyTurn(shooter.angleDeg, shooter.power01, windAtShot,
                               impactPos.x, impactPos.y, craterRadius,
                               dmgAppliedP1, dmgAppliedP2, nextWind, matchOver, winnerPlayer,
                               powerups.ShotPickedType(), powerups.ShotPickedX());
        powerups.ShotPickedType() = -1;
        powerups.ShotPickedX() = 0.0f;
        currentPlayer = netMatch.SyncedCurrentTurnPlayer();

        if (matchOver) {
            if (winnerPlayer != 0) {
                onlineLobby.ReportMatchResult(winnerPlayer == netMatch.MyPlayerNumber());
            }
        } else {
            windForce = nextWind;
        }
    }
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
    if (mode != GameMode::Online) {
        currentPlayer = (currentPlayer == 1) ? 2 : 1;
    }
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;

    if (mode == GameMode::Online) {
        if (version == GameVersion::Plus) {
            OnOnlineTurnCompleted();
        } else {
            onlineCompletedTurns++;
        }
    } else {
        windForce = RandF(-1.0f, 1.0f) *
                    std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());

        if (version == GameVersion::Plus) {
            Cannon& startingCannon = (currentPlayer == 1) ? player1 : player2;
            powerups.TickTurnEffects(startingCannon);

            powerups.TickSpawnCounter();
            powerups.MaybeSpawnRandom();
        }
    }

    stateTimer = 0.4f;
    state = GameState::TurnTransition;
}

