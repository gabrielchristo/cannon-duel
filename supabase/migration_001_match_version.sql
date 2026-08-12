-- Rode isso SÓ SE você já executou o schema.sql anterior (sem a coluna
-- "version" na tabela matches). Se ainda não rodou nada, ignore este
-- arquivo e rode só o schema.sql completo, que já vem com tudo.
alter table matches add column if not exists version text not null default 'classic';
