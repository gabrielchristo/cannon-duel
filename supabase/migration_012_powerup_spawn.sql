-- Spawn autoritativo de power-up por turno (sync via match_turns + fallback HTTP poll).
alter table public.match_turns add column if not exists spawn_powerup_type int not null default -1;
alter table public.match_turns add column if not exists spawn_powerup_x real not null default 0;
