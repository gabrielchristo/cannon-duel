-- Saldo inicial da loja: 300 moedas para jogadores novos.
alter table public.players alter column coins set default 300;
