-- Loja v2: quatro categorias de cosméticos (cor, skin, efeito de canhão, efeito de nome).
alter table public.players add column if not exists equipped_cannon_color text not null default 'color_default';
alter table public.players add column if not exists equipped_cannon_effect text not null default 'effect_default';

alter table public.players alter column equipped_cannon_skin set default 'skin_default';

-- Migra IDs legados (loja v1) para a nova estrutura.
update public.players set equipped_cannon_skin = 'skin_default'
  where equipped_cannon_skin in ('cannon_default', 'cannon_bronze', 'cannon_silver',
                                 'cannon_crimson', 'cannon_gold', 'cannon_void');

update public.player_items set item_id = 'color_blue' where item_id = 'cannon_bronze';
update public.player_items set item_id = 'color_cyan' where item_id = 'cannon_silver';
update public.player_items set item_id = 'color_red' where item_id = 'cannon_crimson';
update public.player_items set item_id = 'color_gold' where item_id = 'cannon_gold';
update public.player_items set item_id = 'color_purple' where item_id = 'cannon_void';

-- Efeitos que vinham embutidos nas skins antigas viram itens separados (posse).
insert into public.player_items (player_id, item_id)
select distinct player_id, 'effect_aura_soft' from public.player_items where item_id = 'color_cyan'
on conflict do nothing;
insert into public.player_items (player_id, item_id)
select distinct player_id, 'effect_aura_fire' from public.player_items where item_id = 'color_red'
on conflict do nothing;
insert into public.player_items (player_id, item_id)
select distinct player_id, 'effect_imbue_holy' from public.player_items where item_id = 'color_gold'
on conflict do nothing;
insert into public.player_items (player_id, item_id)
select distinct player_id, 'effect_arcane' from public.player_items where item_id = 'color_purple'
on conflict do nothing;
