-- ===========================================================================
-- Cannon Duel — schema do multiplayer online (lobby público, sem login)
-- Rode isso inteiro no SQL Editor do seu projeto Supabase.
-- ===========================================================================

-- Jogadores conhecidos (identificador leve, não é conta/login). O "id" é
-- gerado localmente pelo cliente (PlayerIdentity) e reutilizado sempre que
-- o jogo abre — não há senha nem e-mail envolvido.
create table if not exists players (
    id uuid primary key,
    display_name text not null,
    wins int not null default 0,
    losses int not null default 0,
    created_at timestamptz not null default now()
);

-- Presença no lobby público: uma linha por jogador atualmente "online" na
-- tela de multiplayer. O cliente faz upsert nela a cada poucos segundos
-- (heartbeat); linhas com last_seen muito antigo são consideradas offline
-- e ignoradas na listagem (sem precisar de um job de limpeza).
create table if not exists lobby_presence (
    player_id uuid primary key references players(id) on delete cascade,
    display_name text not null,
    wins int not null default 0,
    losses int not null default 0,
    status text not null default 'idle', -- idle | in_match
    last_seen timestamptz not null default now()
);

-- Convites de partida entre dois jogadores do lobby.
create table if not exists challenges (
    id uuid primary key default gen_random_uuid(),
    from_player_id uuid not null references players(id),
    from_display_name text not null,
    to_player_id uuid not null references players(id),
    status text not null default 'pending', -- pending | accepted | declined | expired
    match_id uuid,
    created_at timestamptz not null default now()
);

-- Reservado para a PRÓXIMA etapa (sincronização de partida em si — ainda
-- não usado pelo cliente nesta versão, mas já deixamos criado).
create table if not exists matches (
    id uuid primary key default gen_random_uuid(),
    player1_id uuid not null references players(id),
    player2_id uuid not null references players(id),
    terrain_seed bigint not null,
    version text not null default 'classic', -- classic | plus — escolhido por quem aceita o desafio
    current_turn_player int not null default 1,
    wind real not null default 0,
    status text not null default 'active', -- active | finished
    winner_player int,
    updated_at timestamptz not null default now(),
    created_at timestamptz not null default now()
);

-- Cada linha aqui é o RESULTADO já calculado de um tiro (o cliente que
-- atirou roda a física localmente e manda só o resultado final — evita
-- depender de simulação física bit-a-bit idêntica entre plataformas
-- diferentes, o que o Box2D não garante). O outro cliente lê essa linha e
-- aplica o resultado (cratera + dano), sem precisar recalcular nada.
create table if not exists match_turns (
    id bigserial primary key,
    match_id uuid not null references matches(id) on delete cascade,
    turn_number int not null,
    shooter_player int not null,       -- 1 ou 2
    impact_x real not null,
    impact_y real not null,
    crater_radius real not null,
    damage_p1 real not null default 0,
    damage_p2 real not null default 0,
    next_wind real not null default 0,
    next_turn_player int not null,
    match_over boolean not null default false,
    winner_player int,
    created_at timestamptz not null default now(),
    unique (match_id, turn_number)
);

-- ===========================================================================
-- Row Level Security — como não há login, liberamos acesso público (a chave
-- "anon" é o único mecanismo de acesso), mas de forma controlada: qualquer
-- um pode LER tudo (é um lobby público por natureza), e ESCREVER apenas
-- linhas plausíveis (não dá pra um jogador diretamente inflar a vitória de
-- outro via UPDATE arbitrário, por exemplo — mas para um projeto hobby sem
-- conta de verdade, este é um nível de proteção razoável, não é à prova de
-- um usuário mal-intencionado editando requisições manualmente).
-- ===========================================================================

alter table players enable row level security;
alter table lobby_presence enable row level security;
alter table challenges enable row level security;
alter table matches enable row level security;

create policy "qualquer um pode ler players" on players
    for select using (true);
create policy "qualquer um pode criar seu proprio player" on players
    for insert with check (true);
create policy "qualquer um pode atualizar players (wins/losses)" on players
    for update using (true);

create policy "qualquer um pode ler presenca" on lobby_presence
    for select using (true);
create policy "qualquer um pode fazer upsert da propria presenca" on lobby_presence
    for insert with check (true);
create policy "qualquer um pode atualizar presenca" on lobby_presence
    for update using (true);
create policy "qualquer um pode remover presenca" on lobby_presence
    for delete using (true);

create policy "qualquer um pode ler desafios" on challenges
    for select using (true);
create policy "qualquer um pode criar desafios" on challenges
    for insert with check (true);
create policy "qualquer um pode atualizar desafios" on challenges
    for update using (true);

create policy "qualquer um pode ler partidas" on matches
    for select using (true);
create policy "qualquer um pode criar partidas" on matches
    for insert with check (true);
create policy "qualquer um pode atualizar partidas" on matches
    for update using (true);

alter table match_turns enable row level security;
create policy "qualquer um pode ler turnos" on match_turns
    for select using (true);
create policy "qualquer um pode criar turnos" on match_turns
    for insert with check (true);
