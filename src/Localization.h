#pragma once

enum class Lang { PT_BR, EN };

enum class TK {
    Title,
    OnePlayer,
    TwoPlayers,
    AboutButton,
    ClassicLabel,
    PlusLabel,

    TurnPlayer1,
    TurnPlayer2,
    TurnAI,
    WindLabel,

    MenuButton,
    ResetAngleButton,
    ConfirmTitle,
    ConfirmSub,
    ConfirmYes,
    ConfirmNo,

    RoundOverHint,
    RoundDraw,
    RoundDrawBuried,
    RoundP1Wins,
    RoundP2Wins,
    RoundP1WinsBuried,
    RoundP2WinsBuried,

    AboutTitle,
    AboutBody,
    AboutCredit,
    AboutBack,

    InstructionsButton,
    InstructionsTitle,
    InstructionsBody,
    InstructionsBack,

    PowerupDoubleDamage,
    PowerupTrajectory,
    PowerupGuided,
    PowerupHeal,
    PowerupShield,
};

inline const char* T(TK key, Lang lang) {
    bool pt = (lang == Lang::PT_BR);
    switch (key) {
        case TK::Title: return "CANNON DUEL";
        case TK::OnePlayer: return pt ? "1 JOGADOR (vs IA)" : "1 PLAYER (vs AI)";
        case TK::TwoPlayers: return pt ? "2 JOGADORES" : "2 PLAYERS";
        case TK::AboutButton: return pt ? "SOBRE" : "ABOUT";
        case TK::ClassicLabel: return pt ? "CLASSIC" : "CLASSIC";
        case TK::PlusLabel: return pt ? "PLUS" : "PLUS";

        case TK::TurnPlayer1: return pt ? "Vez do Jogador 1" : "Player 1's turn";
        case TK::TurnPlayer2: return pt ? "Vez do Jogador 2" : "Player 2's turn";
        case TK::TurnAI: return pt ? "Vez da IA" : "AI's turn";
        case TK::WindLabel: return pt ? "VENTO" : "WIND";

        case TK::MenuButton: return pt ? "MENU" : "MENU";
        case TK::ResetAngleButton: return pt ? "RESETAR ANGULO" : "RESET ANGLE";
        case TK::ConfirmTitle: return pt ? "Voltar ao menu inicial?" : "Return to the main menu?";
        case TK::ConfirmSub: return pt ? "A partida atual sera perdida." : "The current match will be lost.";
        case TK::ConfirmYes: return pt ? "SIM, SAIR" : "YES, EXIT";
        case TK::ConfirmNo: return pt ? "CONTINUAR" : "CONTINUE";

        case TK::RoundOverHint: return pt ? "clique para voltar ao menu" : "click to return to the menu";
        case TK::RoundDraw: return pt ? "EMPATE!" : "DRAW!";
        case TK::RoundDrawBuried: return pt ? "EMPATE! (ambos soterrados)" : "DRAW! (both buried)";
        case TK::RoundP1Wins: return pt ? "JOGADOR 1 VENCEU!" : "PLAYER 1 WINS!";
        case TK::RoundP2Wins: return pt ? "JOGADOR 2 VENCEU!" : "PLAYER 2 WINS!";
        case TK::RoundP1WinsBuried: return pt ? "JOGADOR 1 VENCEU! (canhao 2 soterrado)" : "PLAYER 1 WINS! (cannon 2 buried)";
        case TK::RoundP2WinsBuried: return pt ? "JOGADOR 2 VENCEU! (canhao 1 soterrado)" : "PLAYER 2 WINS! (cannon 1 buried)";

        case TK::AboutTitle: return pt ? "SOBRE O JOGO" : "ABOUT THE GAME";
        case TK::AboutBody: return pt
            ? "Cannon Duel e uma releitura moderna, feita do zero, do jogo\n"
              "\"Canhao\", lancado originalmente pela TecToy/Devworks para o\n"
              "Mega Drive em 2005. O objetivo aqui foi recriar a essencia da\n"
              "jogabilidade classica de artilharia por turnos - mira, forca,\n"
              "vento e destruicao de terreno - com fisica realista (Box2D),\n"
              "renderizacao propria (raylib) e uma identidade visual nova.\n"
              "\n"
              "Nenhum grafico, som ou dado original da ROM foi utilizado:\n"
              "toda a arte deste jogo foi criada do zero."
            : "Cannon Duel is a modern reimagining, built from scratch, of\n"
              "\"Canhao\", originally released by TecToy/Devworks for the\n"
              "Mega Drive in 2005. The goal here was to recreate the essence\n"
              "of classic turn-based artillery gameplay - aim, power, wind\n"
              "and terrain destruction - with realistic physics (Box2D),\n"
              "custom rendering (raylib) and a brand new visual identity.\n"
              "\n"
              "No graphics, sound or data from the original ROM were used:\n"
              "all the art in this game was created from scratch.";
        case TK::AboutCredit: return pt ? "Desenvolvido por Gabriel Christo" : "Developed by Gabriel Christo";
        case TK::AboutBack: return pt ? "VOLTAR" : "BACK";

        case TK::InstructionsButton: return pt ? "COMO JOGAR" : "HOW TO PLAY";
        case TK::InstructionsTitle: return pt ? "COMO JOGAR" : "HOW TO PLAY";
        case TK::InstructionsBody: return pt
            ? "MIRA: uma linha oscila entre -90 e 90 graus. Clique (ou\n"
              "espaco no PC) para travar o angulo.\n"
              "\n"
              "FORCA: em seguida, uma barra oscila de fraco (verde) a\n"
              "forte (vermelho). Clique de novo pra travar a forca e atirar.\n"
              "Botao direito (ou B no PC) volta pra selecao de angulo.\n"
              "\n"
              "VENTO: indicado no topo da tela, empurra o projetil durante\n"
              "o voo — sempre existe uma combinacao de angulo/forca capaz\n"
              "de acertar o adversario, mesmo com vento forte.\n"
              "\n"
              "TERRENO: e destrutivel. Cada explosao cava uma cratera.\n"
              "Se o chao embaixo de um canhao for totalmente destruido,\n"
              "esse jogador perde na hora.\n"
              "\n"
              "3 acertos diretos derrubam um canhao.\n"
              "\n"
              "VERSAO PLUS: a cada 2 rodadas aparece um power-up no mapa.\n"
              "Acerte com o seu tiro pra ativar: dano em dobro, trajetoria\n"
              "prevista, tiro teleguiado, cura ou escudo."
            : "AIM: a line oscillates between -90 and 90 degrees. Click\n"
              "(or spacebar on PC) to lock the angle.\n"
              "\n"
              "POWER: next, a bar oscillates from weak (green) to strong\n"
              "(red). Click again to lock the power and fire. Right-click\n"
              "(or B on PC) goes back to angle selection.\n"
              "\n"
              "WIND: shown at the top of the screen, pushes the projectile\n"
              "during flight — there's always some angle/power combo that\n"
              "can hit the opponent, even in strong wind.\n"
              "\n"
              "TERRAIN: fully destructible. Every explosion carves a\n"
              "crater. If the ground under a cannon is completely\n"
              "destroyed, that player loses instantly.\n"
              "\n"
              "3 direct hits take down a cannon.\n"
              "\n"
              "PLUS VERSION: a power-up appears on the map every 2 rounds.\n"
              "Hit it with your shot to activate: double damage, trajectory\n"
              "preview, guided shot, heal, or shield.";
        case TK::InstructionsBack: return pt ? "VOLTAR" : "BACK";

        case TK::PowerupDoubleDamage: return pt ? "DANO EM DOBRO no proximo tiro!" : "DOUBLE DAMAGE on the next shot!";
        case TK::PowerupTrajectory: return pt ? "Trajetoria revelada por 2 rodadas!" : "Trajectory revealed for 2 rounds!";
        case TK::PowerupGuided: return pt ? "Proximo tiro TELEGUIADO (dano menor)!" : "Next shot is GUIDED (lower damage)!";
        case TK::PowerupHeal: return pt ? "Vida recuperada!" : "Health restored!";
        case TK::PowerupShield: return pt ? "ESCUDO ativo por 2 rodadas!" : "SHIELD active for 2 rounds!";

        default: return "";
    }
}
