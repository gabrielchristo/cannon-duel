-- Desafios: formato e versão escolhidos pelo desafiante.
alter table public.challenges
    add column if not exists format text not null default 'duel_1v1',
    add column if not exists version text not null default 'classic';

-- Partidas 2x2: jogadores 3 e 4 + formato.
alter table public.matches
    add column if not exists player3_id uuid references public.players(id),
    add column if not exists player4_id uuid references public.players(id),
    add column if not exists match_format text not null default 'duel_1v1';

-- Sala de equipes entre aceitar desafio 2x2 e iniciar partida.
create table if not exists public.team_rooms (
    id uuid primary key default gen_random_uuid(),
    challenge_id uuid references public.challenges(id) on delete set null,
    captain_a_id uuid not null references public.players(id),
    captain_b_id uuid not null references public.players(id),
    partner_a_id uuid references public.players(id),
    partner_b_id uuid references public.players(id),
    version text not null default 'classic',
    status text not null default 'recruiting',
    match_id uuid references public.matches(id),
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now()
);

-- Convites de parceiro dentro da sala de equipes.
create table if not exists public.team_invites (
    id uuid primary key default gen_random_uuid(),
    room_id uuid not null references public.team_rooms(id) on delete cascade,
    from_player_id uuid not null references public.players(id),
    to_player_id uuid not null references public.players(id),
    team text not null check (team in ('a', 'b')),
    status text not null default 'pending',
    created_at timestamptz not null default now()
);

alter table public.team_rooms enable row level security;
alter table public.team_invites enable row level security;

create policy "qualquer um pode ler team_rooms" on public.team_rooms
    for select using (true);
create policy "qualquer um pode criar team_rooms" on public.team_rooms
    for insert with check (true);
create policy "qualquer um pode atualizar team_rooms" on public.team_rooms
    for update using (true);

create policy "qualquer um pode ler team_invites" on public.team_invites
    for select using (true);
create policy "qualquer um pode criar team_invites" on public.team_invites
    for insert with check (true);
create policy "qualquer um pode atualizar team_invites" on public.team_invites
    for update using (true);

alter table public.team_rooms replica identity full;
alter table public.team_invites replica identity full;

do $$
begin
    if not exists (
        select 1 from pg_publication_tables
        where pubname = 'supabase_realtime' and schemaname = 'public' and tablename = 'team_rooms'
    ) then
        alter publication supabase_realtime add table public.team_rooms;
    end if;
    if not exists (
        select 1 from pg_publication_tables
        where pubname = 'supabase_realtime' and schemaname = 'public' and tablename = 'team_invites'
    ) then
        alter publication supabase_realtime add table public.team_invites;
    end if;
end $$;
