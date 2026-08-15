# Loja

Catálogo cosmético do Cannon Duel. Itens não alteram dano, vida nem física — exceto `ammo_nuclear`, que usa cratera e área de impacto fixas e maiores (`cfg::NUCLEAR_*`).

IDs, preços e o visual descrito aqui são a referência do jogo. Não mudar um ID listado sem pedido explícito. Novos itens entram neste arquivo quando forem adicionados ao catálogo (`src/ShopCatalog.h`).

Alias de carteira: `name_radiant` → `name_cunt`.

Categorias (`ShopCategory`): Cor, Skin, Efeito de canhão, Efeito de nome, Munição.

## Economia

Carteira em `PlayerWallet` (cache local + `players` / `player_items` no Supabase). Saldo inicial: 500 moedas.

| Evento | Moedas | Onde |
|--------|--------|------|
| Pegar power-up | +10 | `GameCombat.cpp` |
| Acerto em inimigo | +15 | `GameCombat.cpp` (popup acima do atirador) |
| Vitória | +50 | `cfg::COINS_ROUND_WIN` |
| Derrota | +5 | `cfg::COINS_ROUND_LOSS` |
| Empate / espectador | 0 | — |

Popup flutuante `+N` em acerto/power-up dura ~3,1s (`CoinPopup`). A tela de fim de partida mostra `Voce ganhou N moedas`.

## Aplicação em partida

- **Local, slots humanos:** usam a carteira do jogador (cor, skin, efeito, munição, efeito de nome). O jogador 2 pode repetir a mesma skin.
- **Local, slots de IA:** sempre itens padrão (`color_default`, `skin_default`, `effect_default`, `ammo_default`, nome `IA`).
- **Online:** cada slot usa as colunas `equipped_*` do dono.
- **Nomes:** sempre visíveis no local (debug e release). Online usa o display name de cada jogador.
- **Power-ups Plus:** dano duplo (orbe/rastro laranja) e teleguiado (roxo) substituem o visual do tiro, não o impacto da munição.

## Como adicionar um item

1. ID + preço + estilo em `src/ShopCatalog.h` (e sprite em `kCannonColorFiles` / `kCannonSkinFiles` / `kAmmoSpriteFiles` se couber).
2. String PT/EN em `src/Localization.h` (`TK` + `T()`). Sem acentos nas strings do jogo.
3. Sprite ou shader: `tools/gen_sprites.py` (cor), `tools/gen_skins.py` (skin), `tools/gen_ammo.py` + `AmmoVisuals.cpp` (munição), `assets/shaders/cosmetic/` (efeito).
4. Registrar o item **neste arquivo** com o mesmo ID.
5. Coluna nova de equipamento só com migration (`equipped_*` em `players`).

## Colunas equipadas (`players`)

| Categoria | Coluna | Padrão |
|-----------|--------|--------|
| Cor | `equipped_cannon_color` | `color_default` |
| Skin | `equipped_cannon_skin` | `skin_default` |
| Efeito de canhão | `equipped_cannon_effect` | `effect_default` |
| Efeito de nome | `equipped_name_effect` | `name_default` |
| Munição | `equipped_ammo` | `ammo_default` (migration 016) |

Migrations da loja: `013_shop`, `014_shop_categories`, `015_starting_coins`, `016_equipped_ammo`.

## Regras visuais

- **Nomes:** um desenho do texto (sem cópia fantasma). Cor forte + contorno preto; aura de shader bem leve. Efeitos (chama, fumaça, pétalas) ao redor.
- **Canhões:** energy coating no recorte do sprite — anel leve + fluxo no casco, sem placa/quadrado. Coating por cima da cor e por baixo da skin. Partículas em `Cannon.cpp`.
- **Shaders:** `assets/shaders/cosmetic/` — um arquivo por efeito + `common.inc`.
- **Skins:** overlays em `assets/sprites/skin_*.png`, gerados por `tools/gen_skins.py`.
- **Cores:** sprites em `assets/sprites/cannon_{cor}.png`, gerados por `tools/gen_sprites.py`.
- **Munição:** sprites em `assets/sprites/ammo_*.png`, gerados por `tools/gen_ammo.py`. Power-ups de dano duplo/teleguiado substituem o visual do tiro (não o impacto da munição).

## Cores de canhão

| ID | Nome (PT) | Sprite | Preço | Paleta |
|----|-----------|--------|-------|--------|
| `color_default` | Padrão | `cannon_left.png` / `cannon_right.png` | 0 | Casco original, sem tint |
| `color_blue` | Azul | `cannon_blue.png` | 25 | accent `{55, 115, 220}` |
| `color_cyan` | Ciano | `cannon_cyan.png` | 25 | accent `{35, 175, 195}` |
| `color_green` | Verde | `cannon_green.png` | 25 | accent `{80, 200, 120}` |
| `color_purple` | Roxo | `cannon_purple.png` | 25 | accent `{155, 75, 195}` |
| `color_red` | Vermelho | `cannon_red.png` | 25 | accent `{215, 65, 55}` |
| `color_orange` | Laranja | `cannon_orange.png` | 25 | accent `{235, 135, 45}` |
| `color_gold` | Dourado | `cannon_gold.png` | 25 | accent `{220, 180, 60}` |
| `color_black` | Preto | `cannon_black.png` | 25 | accent `{45, 48, 55}` |
| `color_tan` | Marrom Claro | `cannon_tan.png` | 25 | accent `{196, 142, 88}` |
| `color_pink` | Rosa | `cannon_pink.png` | 25 | accent `{235, 90, 160}` |

## Skins

Overlay por cima da cor e do coating. Índice `1+` em `ShopItem.skinOverlayIndex` (`kCannonSkinFiles`).

| ID | Nome (PT) | Overlay | Preço | Visual |
|----|-----------|---------|-------|--------|
| `skin_default` | Sem skin | — | 0 | Nenhum overlay |
| `skin_kuromi` | Skin Kuromi | `skin_kuromi.png` | 55 | Laço rosa grande, caveira no nó, orelhas pretas/rosa, blush |
| `skin_gothic` | Skin Gótica | `skin_gothic.png` | 55 | Arco ogival + vitral; cruz latina prata com joia carmim e pontas douradas; espinhos no casco |
| `skin_samurai` | Skin Samurai | `skin_samurai.png` | 70 | Chifres kuwagata dourado/vermelho; sol na torre; faixa; katana no flanco direito (lâmina, tsuba dourada, tsuka enrolada) |
| `skin_pirate` | Skin Pirata | `skin_pirate.png` | 80 | Bandana vermelha na torre, caudas, caveira e brinco dourado |
| `skin_mymelody` | Skin My Melody | `skin_mymelody.png` | 60 | Capuz rosa com orelhas, flor amarela na orelha esquerda, rosto branco, blush |
| `skin_cinnamoroll` | Skin Cinnamoroll | `skin_cinnamoroll.png` | 60 | Orelhas brancas longas, olhos azuis, swirl e nuvens na torre |
| `skin_negaodozap` | Skin Negao do Zap | `skin_negaodozap.png` | 70 | Bucket hat xadrez cinza/preto, toalha verde-agua, rede verde, barra marrom no casco |
| `skin_ninja` | Skin Ninja | `skin_ninja.png` | 70 | Mascara preta, faixa com placa de metal, faixa no casco, shuriken |

## Munição

Visual do projétil, rastro e explosão (`AmmoVisuals.cpp`). Preview na loja usa escala `4.6`. `ammo_67` é desenhado como duas mãos (não o sprite). `ammo_dildo` é desenhado de forma anatômica em roxo claro. Power-up de dano duplo (rastro/orbe laranja) e teleguiado (roxo) substituem o tiro, independente da munição.

| ID | Nome (PT) | Sprite | Preço | Tiro / rastro | Impacto |
|----|-----------|--------|-------|---------------|---------|
| `ammo_default` | Bala Padrao | `projectile.png` | 0 | Esfera padrao | Explosao padrao |
| `ammo_ice` | Estilhaco de Gelo | `ammo_ice.png` | 40 | Cristal ciano | Estilhacos de gelo |
| `ammo_tomato` | Tomate | `ammo_tomato.png` | 35 | Tomate | Polpa vermelha |
| `ammo_duck` | Pato de Borracha | `ammo_duck.png` | 45 | Pato amarelo | Bolhas |
| `ammo_shuriken` | Shuriken | `ammo_shuriken.png` | 55 | Shuriken girando | Faiscas de aco |
| `ammo_67` | 67 | `ammo_67.png` | 67 | Duas maos alternando | Flash amarelo |
| `ammo_kuromi` | Estrela Kuromi | `ammo_kuromi.png` | 70 | Estrela rosa/preta | Explosao roxa |
| `ammo_rasengan` | Rasengan | `ammo_rasengan.png` | 80 | Esfera + espiral azul | Rajada azul |
| `ammo_chidori` | Chidori | `ammo_chidori.png` | 80 | Orbe + relampagos em zigue-zague | Descarga eletrica |
| `ammo_pride` | Bola Cunt | `ammo_pride.png` | 85 | Bola pride | Explosao pride |
| `ammo_dildo` | Dildo | `ammo_dildo.png` | 50 | Membro roxo claro | Explosao lilas |
| `ammo_nuclear` | Bomba Nuclear | `ammo_nuclear.png` | 10000 | Bomba | Cogumelo enorme; cratera/area fixas grandes |

## Efeitos de nome

Shaders em `assets/shaders/cosmetic/name_*.fs`. Ornamentos CPU em `CosmeticShaders::DrawNameOrnaments`.

| ID | Nome (PT) | Estilo | Preço | Shader | Visual |
|----|-----------|--------|-------|--------|--------|
| `name_default` | Nome Padrão | `Plain` | 0 | — | Texto da cor do jogador, sem efeito |
| `name_ember` | Nome em Brasa | `Flame` | 20 | `name_ember.fs` | Letras em brasa (laranja→amarelo); línguas de fogo no halo (`DrawEmberFire`) |
| `name_shadow` | Nome Sombrio | `DarkSmoke` | 30 | `name_shadow.fs` | Letras prata; fumaça cinza subindo à esquerda |
| `name_amethyst` | Nome Ametista | `PurpleGlow` | 45 | `name_amethyst.fs` | Roxo/magenta facetado que pulsa; motes em órbita |
| `name_ocean` | Nome Oceano | `OceanWave` | 60 | `name_ocean.fs` | Azul→ciano em ondas; cristas claras; motes em seno horizontal |
| `name_sakura` | Nome Sakura | `Sakura` | 70 | `name_sakura.fs` | Rosa oscilando; pétalas caindo em elipse |
| `name_cunt` | Nome Cunt | `Cunt` | 85 | `name_cunt.fs` | Gradiente pride esquerda→direita rolando rápido (vermelho, laranja, amarelo, verde, azul, roxo); motes pride acima do nome |

## Efeitos de canhão

Camadas: energy coating (`cannon_*.fs`) + partículas em `DrawCannonCosmeticEffect`. Contorno leve no recorte do sprite. Sem disco/quadrado.

| ID | Nome (PT) | Estilo | Preço | Shader | Coating | Partículas |
|----|-----------|--------|-------|--------|---------|------------|
| `effect_default` | Sem Efeito | `None` | 0 | — | — | — |
| `effect_aura_soft` | Aura + Ego | `AuraSoft` | 35 | `cannon_aura_soft.fs` | Azul-ciano, fluxo suave para cima | Coluna de 10 motes subindo devagar, pouco drift |
| `effect_aura_fire` | Aura de Fogo | `AuraFire` | 45 | `cannon_aura_fire.fs` | Laranja-vermelho, fluxo ascendente oscilante | 9 línguas que sobem do casco e balançam para os lados (laranja + ponta amarela) |
| `effect_debuff_glow` | Brilho Envenenado | `DebuffGlow` | 50 | `cannon_debuff.fs` | Verde tóxico | 8 gotas verdes descendo (`40,210,70` + slime `90,255,120`) |
| `effect_imbue_holy` | Imbuimento Sagrado | `ImbueHoly` | 60 | `cannon_holy.fs` | Dourado, pulso radial | 8 motes em auréola (pulso + subida curta, núcleo branco) |
| `effect_67` | 67 | `SixSeven` | 67 | `cannon_67.fs` | Dourado, fluxo que balanca | Duas maos junto do casco, subindo e descendo em alternancia |
| `effect_arcane` | Faísca Arcana | `ArcaneSpark` | 75 | `cannon_arcane.fs` | Roxo, fluxo em twist | 14 faíscas em espiral |
| `effect_aura_cunt` | Aura Cunt | `AuraCunt` | 80 | `cannon_cunt.fs` | Aura líquida arco-íris (`prideRamp` + `fluidField`) no recorte do casco | Manchas líquidas + motes pride orbitando |
| `effect_liquid_frost` | Gelo Líquido | `LiquidFrost` | 85 | `cannon_frost.fs` | Ciano-gelo, fluxo horizontal | 8 cristais avançando para os lados e piscando |
| `effect_liquid_inferno` | Inferno Líquido | `LiquidInferno` | 90 | `cannon_inferno.fs` | Vermelho-fogo, advecção para cima | 11 labaredas acelerando das esteiras |
| `effect_kyuubi` | Kyuubi | `Kyuubi` | 95 | `cannon_kyuubi.fs` | Vermelho-chakra, fluxo ascendente | 9 caudas + muitas particulas atras do canhao |
| `effect_liquid_void` | Vazio Líquido | `LiquidVoid` | 100 | `cannon_void.fs` | Roxo-violeta, fluxo para o centro | 12 motes em órbita sugados para o centro |

## Arquivos

| Peça | Onde |
|------|------|
| Catálogo / preços / IDs | `src/ShopCatalog.h` |
| Strings PT/EN | `src/Localization.h` |
| UI da loja | `src/GameShop.cpp` |
| Carteira / compra / equip | `src/net/PlayerWallet.cpp` |
| Cosméticos no canhão | `src/Cannon.cpp`, `src/GameCore.cpp` (`ApplyEquippedCosmetics`) |
| Tiro / rastro / impacto | `src/AmmoVisuals.cpp` |
| Popup e fim de partida | `src/CoinPopup.cpp`, `src/GameDraw.cpp` |
| Shaders | `assets/shaders/cosmetic/` |
| Cores | `tools/gen_sprites.py` → `cannon_{cor}.png` |
| Skins | `tools/gen_skins.py` → `skin_*.png` |
| Munição | `tools/gen_ammo.py` → `ammo_*.png` |
| Schema | `supabase/migration_013_shop.sql` … `016_equipped_ammo.sql` |
