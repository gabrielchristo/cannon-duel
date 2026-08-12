#pragma once
#include <string>
#include <vector>

// Buffer de log temporário pro overlay na tela (Android sem logcat à mão).
// Deliberadamente INDEPENDENTE do TraceLog do raylib — não depende do
// nível de log configurado nem de flags de build (ex: SUPPORT_TRACELOG),
// que podem variar entre a build desktop e a Android e filtrar mensagens
// silenciosamente antes de chegarem em qualquer callback.
namespace DebugLog {
    void Push(const std::string& line);
    const std::vector<std::string>& Lines();
}

// Loga formatado, tanto no TraceLog padrão do raylib (stdout/logcat,
// quando disponível) quanto no buffer acima. Use isso em vez de
// TraceLog(...) diretamente nos arquivos de rede — garante que a mensagem
// apareça no overlay independente de qualquer configuração de log.
void DebugLogf(int level, const char* fmt, ...);
