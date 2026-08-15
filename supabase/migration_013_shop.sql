-- Loja: moedas e cosméticos (skin do canhão + efeito de nome).
alter table public.players add column if not exists coins int not null default 0;
alter table public.players add column if not exists equipped_cannon_skin text not null default 'cannon_default';
alter table public.players add column if not exists equipped_name_effect text not null default 'name_default';

-- Itens comprados pelo jogador (histórico de posse — usado pra liberar
-- reequipar sem pagar de novo).
create table if not exists public.player_items (
    player_id uuid not null references public.players(id) on delete cascade,
    item_id text not null,
    purchased_at timestamptz not null default now(),
    primary key (player_id, item_id)
);

alter table public.player_items enable row level security;

create policy "qualquer um pode ler itens" on public.player_items
    for select using (true);
create policy "qualquer um pode comprar item" on public.player_items
    for insert with check (true);
