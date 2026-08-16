-- Saldo inicial da loja: 500 moedas para jogadores novos e quem ainda está em 0.
alter table public.players alter column coins set default 500;
update public.players set coins = 500 where coins <= 0;
