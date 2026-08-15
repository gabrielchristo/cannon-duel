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
    RoundOverRematch,
    RoundOverBackToLobby,
    RoundDraw,
    RoundDrawBuried,
    RoundP1Wins,
    RoundP2Wins,
    RoundP1WinsBuried,
    RoundP2WinsBuried,
    RoundTeamAWins,
    RoundTeamBWins,
    RoundTeamAWinsBuried,
    RoundTeamBWinsBuried,

    FormatSelectTitle,
    FormatDuel1v1,
    FormatTeam2v2,
    FormatBack,
    TurnTeamFmt,

    RoundOpponentDisconnected,

    AboutTitle,
    AboutBody,
    AboutCredit,
    AboutBack,

    InstructionsButton,
    InstructionsTitle,
    InstructionsBody,
    InstructionsBack,

    RoundOverSpectatorHint,

    OnlineWatchButton,
    OnlinePlayingLabel,
    OnlineTeamPrepLabel,
    OnlineSpectatingFmt,
    OnlineSpectatingTeamFmt,
    RoundSpectatorMatchOver,

    OnlineButton,
    OnlineTabLobby,
    OnlineTabMatches,
    OnlineNoActiveMatches,
    OnlineMatchTurnFmt,

    OnlineTitle,
    OnlineYourName,
    OnlineNoPlayers,
    OnlineChallengeSent,
    OnlineChallengeButton,
    OnlineIncomingChallenge,
    OnlineAcceptButton,
    OnlineDeclineButton,
    OnlineBack,
    OnlineRecordFmt,
    OnlineWaitingSuffix,
    OnlineEditNameButton,
    OnlineNameSave,
    OnlineNameCancel,
    OnlineNameHint,

    OnlineFormat1v1,
    OnlineFormat2v2,
    OnlineChallengeSentFmt,
    OnlineIncomingChallengeFmt,
    OnlineTeamRoomTitle,
    OnlineTeamInviteButton,
    OnlineTeamInviteIncoming,
    OnlineTeamCancel,
    OnlineTeamStart,
    OnlineTeamSlotEmpty,
    OnlineTeamLabelA,
    OnlineTeamLabelB,
    OnlineTeamWaitingPartners,
    OnlineTeamLeave,
    OnlineTeamLobbyInviteTitle,
    OnlineTeamCompositionHint,
    OnlineChallengeSentVersionFmt,
    OnlineTeamInviteVersionFmt,
    OnlineChallengeDeclinedFmt,
    OnlineChallengeExpiredFmt,

    PowerupDoubleDamage,
    PowerupTrajectory,
    PowerupGuided,
    PowerupHeal,
    PowerupShield,

    ShopButton,
    ShopTitle,
    ShopBack,
    ShopTabColors,
    ShopTabSkins,
    ShopTabEffects,
    ShopTabNames,
    ShopTabAmmo,
    ShopNamePreviewSample,
    ShopBuyButton,
    ShopEquipButton,
    ShopEquippedLabel,
    ShopNotEnoughCoins,
    ShopCoinsLabel,

    ShopItemDefaultColor,
    ShopColorBlue,
    ShopColorCyan,
    ShopColorGreen,
    ShopColorPurple,
    ShopColorRed,
    ShopColorOrange,
    ShopColorGold,
    ShopColorBlack,
    ShopColorTan,

    ShopItemDefaultSkin,
    ShopSkinKuromi,
    ShopSkinGothic,
    ShopSkinSamurai,
    ShopSkinPirate,
    ShopSkinMyMelody,
    ShopSkinCinnamoroll,
    ShopSkinNegaoDoZap,
    ShopSkinNinja,

    ShopItemDefaultEffect,
    ShopEffectAuraSoft,
    ShopEffectAuraFire,
    ShopEffectImbueHoly,
    ShopEffectDebuffGlow,
    ShopEffectArcane,
    ShopEffectLiquidInferno,
    ShopEffectLiquidFrost,
    ShopEffectLiquidVoid,
    ShopEffectAuraCunt,
    ShopEffectSixSeven,
    ShopEffectKyuubi,

    ShopItemDefaultName,
    ShopItemEmberName,
    ShopItemShadowName,
    ShopItemAmethystName,
    ShopItemCuntName,
    ShopItemOceanName,
    ShopItemSakuraName,

    ShopAmmoDefault,
    ShopAmmoIce,
    ShopAmmoRasengan,
    ShopAmmoChidori,
    ShopAmmoShuriken,
    ShopAmmoKuromi,
    ShopAmmoPride,
    ShopAmmoSixSeven,
    ShopAmmoTomato,
    ShopAmmoDuck,
    ShopAmmoNuclear,
};

inline const char* T(TK key, Lang lang) {
    bool pt = (lang == Lang::PT_BR);
    switch (key) {
        case TK::Title: return "CANNON DUEL";
        case TK::OnePlayer: return pt ? "1 JOGADOR" : "1 PLAYER";
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
        case TK::RoundOverRematch: return pt ? "JOGAR NOVAMENTE" : "PLAY AGAIN";
        case TK::RoundOverBackToLobby: return pt ? "VOLTAR AO LOBBY" : "BACK TO LOBBY";
        case TK::RoundOverSpectatorHint: return pt ? "clique para voltar ao lobby" : "click to return to the lobby";
        case TK::RoundDraw: return pt ? "EMPATE!" : "DRAW!";
        case TK::RoundDrawBuried: return pt ? "EMPATE! (ambos soterrados)" : "DRAW! (both buried)";
        case TK::RoundP1Wins: return pt ? "JOGADOR 1 VENCEU!" : "PLAYER 1 WINS!";
        case TK::RoundP2Wins: return pt ? "JOGADOR 2 VENCEU!" : "PLAYER 2 WINS!";
        case TK::RoundP1WinsBuried: return pt ? "JOGADOR 1 VENCEU! (canhao 2 soterrado)" : "PLAYER 1 WINS! (cannon 2 buried)";
        case TK::RoundP2WinsBuried: return pt ? "JOGADOR 2 VENCEU! (canhao 1 soterrado)" : "PLAYER 2 WINS! (cannon 1 buried)";
        case TK::RoundTeamAWins: return pt ? "EQUIPE A VENCEU!" : "TEAM A WINS!";
        case TK::RoundTeamBWins: return pt ? "EQUIPE B VENCEU!" : "TEAM B WINS!";
        case TK::RoundTeamAWinsBuried: return pt ? "EQUIPE A VENCEU! (equipe B soterrada)" : "TEAM A WINS! (team B buried)";
        case TK::RoundTeamBWinsBuried: return pt ? "EQUIPE B VENCEU! (equipe A soterrada)" : "TEAM B WINS! (team A buried)";
        case TK::FormatSelectTitle: return pt ? "ESCOLHA O FORMATO" : "CHOOSE FORMAT";
        case TK::FormatDuel1v1: return pt ? "DUELO 1 x 1" : "1 vs 1 DUEL";
        case TK::FormatTeam2v2: return pt ? "EQUIPE 2 x 2" : "TEAM 2 vs 2";
        case TK::FormatBack: return pt ? "VOLTAR" : "BACK";
        case TK::TurnTeamFmt: return pt ? "Equipe %s - Canhao %d" : "Team %s - Cannon %d";
        case TK::RoundOpponentDisconnected: return pt ? "VITORIA! Adversario desconectou." : "YOU WIN! Opponent disconnected.";

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

        case TK::OnlineButton: return pt ? "MULTIPLAYER ONLINE" : "ONLINE MULTIPLAYER";
        case TK::OnlineWatchButton: return pt ? "ASSISTIR" : "WATCH";
        case TK::OnlinePlayingLabel: return pt ? "JOGANDO" : "PLAYING";
        case TK::OnlineTeamPrepLabel: return pt ? "PREPARANDO" : "PREPARING";
        case TK::OnlineSpectatingFmt: return pt ? "Assistindo: %s vs %s" : "Spectating: %s vs %s";
        case TK::OnlineSpectatingTeamFmt:
            return pt ? "Assistindo: %s & %s vs %s & %s" : "Spectating: %s & %s vs %s & %s";
        case TK::RoundSpectatorMatchOver: return pt ? "PARTIDA ENCERRADA" : "MATCH OVER";
        case TK::OnlineTabLobby: return pt ? "LOBBY" : "LOBBY";
        case TK::OnlineTabMatches: return pt ? "PARTIDAS" : "MATCHES";
        case TK::OnlineNoActiveMatches: return pt
            ? "Nenhuma partida ao vivo no momento."
            : "No live matches right now.";
        case TK::OnlineMatchTurnFmt: return pt ? "Vez do jogador %d" : "Player %d's turn";
        case TK::OnlineTitle: return pt ? "LOBBY ONLINE" : "ONLINE LOBBY";
        case TK::OnlineYourName: return pt ? "Voce:" : "You:";
        case TK::OnlineNoPlayers: return pt
            ? "Nenhum jogador online no momento. Espere um pouco ou chame um amigo!"
            : "No players online right now. Wait a bit or invite a friend!";
        case TK::OnlineChallengeSent: return pt ? "Desafio enviado! Aguardando resposta..." : "Challenge sent! Waiting for response...";
        case TK::OnlineChallengeButton: return pt ? "DESAFIAR" : "CHALLENGE";
        case TK::OnlineIncomingChallenge: return pt ? "te desafiou!" : "challenged you!";
        case TK::OnlineAcceptButton: return pt ? "ACEITAR" : "ACCEPT";
        case TK::OnlineDeclineButton: return pt ? "RECUSAR" : "DECLINE";
        case TK::OnlineBack: return pt ? "VOLTAR" : "BACK";
        case TK::OnlineRecordFmt: return pt ? "%d vitorias / %d derrotas" : "%d wins / %d losses";
        case TK::OnlineWaitingSuffix: return pt ? " esta jogando..." : " is playing...";
        case TK::OnlineEditNameButton: return pt ? "EDITAR NOME" : "EDIT NAME";
        case TK::OnlineNameSave: return pt ? "SALVAR" : "SAVE";
        case TK::OnlineNameCancel: return pt ? "CANCELAR" : "CANCEL";
        case TK::OnlineNameHint: return pt ? "Max. 24 caracteres" : "Max. 24 characters";

        case TK::OnlineFormat1v1: return pt ? "1 x 1" : "1 x 1";
        case TK::OnlineFormat2v2: return pt ? "2 x 2" : "2 x 2";
        case TK::OnlineChallengeSentFmt: return pt ? "Desafio enviado (%s · %s) — aguardando..."
            : "Challenge sent (%s · %s) — waiting...";
        case TK::OnlineIncomingChallengeFmt: return pt ? "te desafiou · %s · %s"
            : "challenged you · %s · %s";
        case TK::OnlineTeamRoomTitle: return pt ? "SALA DE COMPOSICAO" : "COMPOSITION ROOM";
        case TK::OnlineTeamInviteButton: return pt ? "CONVIDAR" : "INVITE";
        case TK::OnlineTeamInviteIncoming: return pt ? "convida voce para a equipe" : "invites you to the team";
        case TK::OnlineTeamInviteVersionFmt: return pt ? "Modo: %s" : "Mode: %s";
        case TK::OnlineTeamCancel: return pt ? "CANCELAR" : "CANCEL";
        case TK::OnlineTeamStart: return pt ? "INICIAR PARTIDA" : "START MATCH";
        case TK::OnlineTeamSlotEmpty: return pt ? "vaga aberta" : "open slot";
        case TK::OnlineTeamLabelA: return pt ? "EQUIPE A" : "TEAM A";
        case TK::OnlineTeamLabelB: return pt ? "EQUIPE B" : "TEAM B";
        case TK::OnlineTeamWaitingPartners: return pt ? "Aguardando parceiros..." : "Waiting for partners...";
        case TK::OnlineTeamLeave: return pt ? "SAIR DA EQUIPE" : "LEAVE TEAM";
        case TK::OnlineTeamLobbyInviteTitle: return pt ? "Convidar do lobby:" : "Invite from lobby:";
        case TK::OnlineTeamCompositionHint: return pt
            ? "Convide parceiros ou inicie quando quiser (ate 5/equipe)"
            : "Invite partners or start anytime (up to 5/team)";
        case TK::OnlineChallengeSentVersionFmt: return pt
            ? "Desafio enviado (%s) — aguardando..."
            : "Challenge sent (%s) — waiting...";
        case TK::OnlineChallengeDeclinedFmt: return pt
            ? "%s recusou o desafio."
            : "%s declined the challenge.";
        case TK::OnlineChallengeExpiredFmt: return pt
            ? "Desafio para %s expirou (sem resposta)."
            : "Challenge to %s expired (no response).";

        case TK::PowerupDoubleDamage: return pt ? "DANO EM DOBRO no proximo tiro!" : "DOUBLE DAMAGE on the next shot!";
        case TK::PowerupTrajectory: return pt ? "Trajetoria revelada por 1 rodada!" : "Trajectory revealed for 1 round!";
        case TK::PowerupGuided: return pt ? "Proximo tiro TELEGUIADO (dano menor)!" : "Next shot is GUIDED (lower damage)!";
        case TK::PowerupHeal: return pt ? "Vida recuperada!" : "Health restored!";
        case TK::PowerupShield: return pt ? "ESCUDO ativo por 1 turno!" : "SHIELD active for 1 turn!";

        case TK::ShopButton: return pt ? "LOJA" : "SHOP";
        case TK::ShopTitle: return pt ? "LOJA" : "SHOP";
        case TK::ShopBack: return pt ? "VOLTAR" : "BACK";
        case TK::ShopTabColors: return pt ? "CORES" : "COLORS";
        case TK::ShopTabSkins: return pt ? "SKINS" : "SKINS";
        case TK::ShopTabEffects: return pt ? "EFEITOS" : "EFFECTS";
        case TK::ShopTabNames: return pt ? "NOMES" : "NAMES";
        case TK::ShopTabAmmo: return pt ? "MUNICAO" : "AMMO";
        case TK::ShopNamePreviewSample: return pt ? "SEU NOME" : "YOUR NAME";
        case TK::ShopBuyButton: return pt ? "COMPRAR" : "BUY";
        case TK::ShopEquipButton: return pt ? "EQUIPAR" : "EQUIP";
        case TK::ShopEquippedLabel: return pt ? "EQUIPADO" : "EQUIPPED";
        case TK::ShopNotEnoughCoins: return pt ? "Moedas insuficientes" : "Not enough coins";
        case TK::ShopCoinsLabel: return pt ? "MOEDAS" : "COINS";

        case TK::ShopItemDefaultColor: return pt ? "Branco (Padrao)" : "White (Default)";
        case TK::ShopColorBlue: return pt ? "Azul" : "Blue";
        case TK::ShopColorCyan: return pt ? "Ciano" : "Cyan";
        case TK::ShopColorGreen: return pt ? "Verde" : "Green";
        case TK::ShopColorPurple: return pt ? "Roxo" : "Purple";
        case TK::ShopColorRed: return pt ? "Vermelho" : "Red";
        case TK::ShopColorOrange: return pt ? "Laranja" : "Orange";
        case TK::ShopColorGold: return pt ? "Dourado" : "Gold";
        case TK::ShopColorBlack: return pt ? "Preto" : "Black";
        case TK::ShopColorTan: return pt ? "Marrom Claro" : "Light Brown";

        case TK::ShopItemDefaultSkin: return pt ? "Sem Skin" : "No Skin";
        case TK::ShopSkinKuromi: return pt ? "Skin Kuromi" : "Kuromi Skin";
        case TK::ShopSkinGothic: return pt ? "Skin Gotica" : "Gothic Skin";
        case TK::ShopSkinSamurai: return pt ? "Skin Samurai" : "Samurai Skin";
        case TK::ShopSkinPirate: return pt ? "Skin Pirata" : "Pirate Skin";
        case TK::ShopSkinMyMelody: return pt ? "Skin My Melody" : "My Melody Skin";
        case TK::ShopSkinCinnamoroll: return pt ? "Skin Cinnamoroll" : "Cinnamoroll Skin";
        case TK::ShopSkinNegaoDoZap: return pt ? "Skin Negao do Zap" : "Negao do Zap Skin";
        case TK::ShopSkinNinja: return pt ? "Skin Ninja" : "Ninja Skin";

        case TK::ShopItemDefaultEffect: return pt ? "Sem Efeito" : "No Effect";
        case TK::ShopEffectAuraSoft: return pt ? "Aura + Ego" : "Aura + Ego";
        case TK::ShopEffectAuraFire: return pt ? "Aura de Fogo" : "Fire Aura";
        case TK::ShopEffectImbueHoly: return pt ? "Imbuimento Sagrado" : "Holy Imbue";
        case TK::ShopEffectDebuffGlow: return pt ? "Brilho Envenenado" : "Poison Glow";
        case TK::ShopEffectArcane: return pt ? "Faisca Arcana" : "Arcane Spark";
        case TK::ShopEffectLiquidInferno: return pt ? "Inferno Liquido" : "Liquid Inferno";
        case TK::ShopEffectLiquidFrost: return pt ? "Gelo Liquido" : "Liquid Frost";
        case TK::ShopEffectLiquidVoid: return pt ? "Vazio Liquido" : "Liquid Void";
        case TK::ShopEffectAuraCunt: return pt ? "Aura Cunt" : "Cunt Aura";
        case TK::ShopEffectSixSeven: return pt ? "67" : "Six Seven";
        case TK::ShopEffectKyuubi: return pt ? "Kyuubi" : "Kyuubi";

        case TK::ShopItemDefaultName: return pt ? "Nome Padrao" : "Standard Name";
        case TK::ShopItemEmberName: return pt ? "Nome em Brasa" : "Ember Name";
        case TK::ShopItemShadowName: return pt ? "Nome Sombrio" : "Shadow Name";
        case TK::ShopItemAmethystName: return pt ? "Nome Ametista" : "Amethyst Name";
        case TK::ShopItemCuntName: return pt ? "Nome Cunt" : "Cunt Name";
        case TK::ShopItemOceanName: return pt ? "Nome Oceano" : "Ocean Name";
        case TK::ShopItemSakuraName: return pt ? "Nome Sakura" : "Sakura Name";

        case TK::ShopAmmoDefault: return pt ? "Bala Padrao" : "Standard Shot";
        case TK::ShopAmmoIce: return pt ? "Estilhaco de Gelo" : "Ice Shard";
        case TK::ShopAmmoRasengan: return pt ? "Rasengan" : "Rasengan";
        case TK::ShopAmmoChidori: return pt ? "Chidori" : "Chidori";
        case TK::ShopAmmoShuriken: return pt ? "Shuriken" : "Shuriken";
        case TK::ShopAmmoKuromi: return pt ? "Estrela Kuromi" : "Kuromi Star";
        case TK::ShopAmmoPride: return pt ? "Bola Cunt" : "Cunt Ball";
        case TK::ShopAmmoSixSeven: return pt ? "Emoji 67" : "67 Emoji";
        case TK::ShopAmmoTomato: return pt ? "Tomate" : "Tomato";
        case TK::ShopAmmoDuck: return pt ? "Pato de Borracha" : "Rubber Duck";
        case TK::ShopAmmoNuclear: return pt ? "Bomba Nuclear" : "Nuclear Bomb";

        default: return "";
    }
}
