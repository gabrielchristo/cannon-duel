-- Munição cosmética equipada (exceto nuclear, que amplia cratera/área).
alter table public.players
  add column if not exists equipped_ammo text not null default 'ammo_default';
