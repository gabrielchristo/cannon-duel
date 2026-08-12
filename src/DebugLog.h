#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// Buffer de log pro overlay na tela (Android sem logcat à mão).
// InstallOverlayCapture() redireciona TraceLog, printf e logcat Android
// pra cá — nada vai mais pro terminal/logcat.
namespace DebugLog {
    void Push(const std::string& line);
    const std::vector<std::string>& Lines();

    // Redireciona TraceLog (raylib), printf e logcat Android pro buffer do
    // overlay — nada vai mais pro terminal/logcat. Chamar uma vez no boot.
    void InstallOverlayCapture();
}

// Loga formatado no overlay via TraceLog + callback instalado por
// InstallOverlayCapture(). Use em vez de TraceLog/printf diretos.
void DebugLogf(int level, const char* fmt, ...);
