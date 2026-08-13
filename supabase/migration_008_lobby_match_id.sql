-- Presença no lobby: qual partida ativa o jogador está assistindo/jogando.
alter table public.lobby_presence
    add column if not exists match_id uuid references public.matches(id) on delete set null;
