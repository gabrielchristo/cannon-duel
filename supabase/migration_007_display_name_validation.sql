-- Validação de display_name no banco (defesa em profundidade).
-- O cliente também sanitiza antes de enviar via PostgREST (JSON parametrizado).

alter table players
    drop constraint if exists players_display_name_length;

alter table players
    add constraint players_display_name_length
    check (char_length(display_name) between 1 and 24);

alter table lobby_presence
    drop constraint if exists lobby_presence_display_name_length;

alter table lobby_presence
    add constraint lobby_presence_display_name_length
    check (char_length(display_name) between 1 and 24);
