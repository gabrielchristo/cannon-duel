#pragma once

// ===========================================================================
// PREENCHA com os dados do SEU projeto Supabase (Project Settings → API).
// A "anon key" é feita pra ser pública/embutida no cliente — a segurança
// vem das políticas de RLS configuradas no banco (ver supabase/schema.sql),
// não do sigilo dessa chave.
// ===========================================================================
namespace supabase_config {
    inline constexpr const char* URL = "https://smnabmgxxhznioqmdxin.supabase.co";
    inline constexpr const char* ANON_KEY = "sb_publishable_LLcxHQCgwtTkW9xnl9s3qw_P7RnGaQJ";
}
