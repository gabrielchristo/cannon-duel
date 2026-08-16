-- Dano por canhão em partidas 2x2 (slots 3 e 4).
alter table public.match_turns
    add column if not exists damage_p3 real not null default 0,
    add column if not exists damage_p4 real not null default 0;
