-- Power-up coletado durante o tiro (sincronizado no adversário).
-- picked_powerup_type: -1 = nenhum; 0..N = enum PowerupType.
alter table public.match_turns add column if not exists picked_powerup_type int not null default -1;
alter table public.match_turns add column if not exists picked_powerup_x real not null default 0;
