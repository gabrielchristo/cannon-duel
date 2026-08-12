-- Campos de mira/vento no momento do tiro — permite replay visual no cliente
-- adversário (projétil, linha de ângulo, barra de força).
alter table match_turns add column if not exists shoot_angle real not null default 45;
alter table match_turns add column if not exists shoot_power real not null default 0.5;
alter table match_turns add column if not exists wind_at_shot real not null default 0;
