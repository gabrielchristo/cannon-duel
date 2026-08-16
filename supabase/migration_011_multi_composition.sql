-- Composição dinâmica (até 5v5) na sala de equipes.
create table if not exists public.team_room_members (
    id uuid primary key default gen_random_uuid(),
    room_id uuid not null references public.team_rooms(id) on delete cascade,
    team text not null check (team in ('a', 'b')),
    slot int not null check (slot >= 0 and slot <= 4),
    player_id uuid not null references public.players(id),
    created_at timestamptz not null default now(),
    unique (room_id, team, slot),
    unique (room_id, player_id)
);

alter table public.team_room_members enable row level security;

create policy "qualquer um pode ler team_room_members" on public.team_room_members
    for select using (true);
create policy "qualquer um pode criar team_room_members" on public.team_room_members
    for insert with check (true);
create policy "qualquer um pode atualizar team_room_members" on public.team_room_members
    for update using (true);
create policy "qualquer um pode deletar team_room_members" on public.team_room_members
    for delete using (true);

alter table public.team_room_members replica identity full;

do $$
begin
    if not exists (
        select 1 from pg_publication_tables
        where pubname = 'supabase_realtime' and schemaname = 'public'
          and tablename = 'team_room_members'
    ) then
        alter publication supabase_realtime add table public.team_room_members;
    end if;
end $$;

-- Partidas até 10 jogadores + contagem por equipe.
alter table public.matches
    add column if not exists player5_id uuid references public.players(id),
    add column if not exists player6_id uuid references public.players(id),
    add column if not exists player7_id uuid references public.players(id),
    add column if not exists player8_id uuid references public.players(id),
    add column if not exists player9_id uuid references public.players(id),
    add column if not exists player10_id uuid references public.players(id),
    add column if not exists team_a_count int not null default 1,
    add column if not exists team_b_count int not null default 1;

-- Dano por canhão (slots 5–10).
alter table public.match_turns
    add column if not exists damage_p5 real not null default 0,
    add column if not exists damage_p6 real not null default 0,
    add column if not exists damage_p7 real not null default 0,
    add column if not exists damage_p8 real not null default 0,
    add column if not exists damage_p9 real not null default 0,
    add column if not exists damage_p10 real not null default 0;

-- Migrar salas existentes (capitães + parceiros legados) para team_room_members.
insert into public.team_room_members (room_id, team, slot, player_id)
select tr.id, 'a', 0, tr.captain_a_id from public.team_rooms tr
where tr.captain_a_id is not null
on conflict (room_id, team, slot) do nothing;

insert into public.team_room_members (room_id, team, slot, player_id)
select tr.id, 'b', 0, tr.captain_b_id from public.team_rooms tr
where tr.captain_b_id is not null
on conflict (room_id, team, slot) do nothing;

insert into public.team_room_members (room_id, team, slot, player_id)
select tr.id, 'a', 1, tr.partner_a_id from public.team_rooms tr
where tr.partner_a_id is not null
on conflict (room_id, team, slot) do nothing;

insert into public.team_room_members (room_id, team, slot, player_id)
select tr.id, 'b', 1, tr.partner_b_id from public.team_rooms tr
where tr.partner_b_id is not null
on conflict (room_id, team, slot) do nothing;
