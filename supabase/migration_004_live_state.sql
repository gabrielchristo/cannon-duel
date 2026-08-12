-- Estado ao vivo da mira/tiro + heartbeat por jogador na partida.
alter table matches add column if not exists live_shooter int not null default 0;
alter table matches add column if not exists live_angle real not null default 45;
alter table matches add column if not exists live_power real not null default 0.5;
alter table matches add column if not exists live_aim_phase text not null default 'idle';
alter table matches add column if not exists live_wind real not null default 0;
alter table matches add column if not exists live_shot_id int not null default 0;
alter table matches add column if not exists p1_last_seen timestamptz;
alter table matches add column if not exists p2_last_seen timestamptz;
