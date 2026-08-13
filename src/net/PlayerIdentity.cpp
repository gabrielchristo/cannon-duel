#include "PlayerIdentity.h"
#include "NetValidation.h"
#include "../Platform.h"
#include "../DebugLog.h"

#include <raylib.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <random>

#if CANNON_DUEL_WEB_BUILD
#include "WebIdbfs.h"
#endif

std::string PlayerIdentity::SavePath() {
    // Desktop: ao lado do executável.
    // Android: GetWorkingDirectory() aponta pro storage interno do app
    // (sobrevive entre sessões; reinstall limpa — esperado).
    // Web: MEMFS não sobrevive a reload — "/idbfs" é espelhado pra
    // IndexedDB via WebIdbfsMountAndSyncIn()/WebIdbfsSyncOut().
#if CANNON_DUEL_ANDROID_BUILD
    return std::string(GetWorkingDirectory()) + "/player_identity.txt";
#elif CANNON_DUEL_WEB_BUILD
    return "/idbfs/player_identity.txt";
#else
    return "player_identity.txt";
#endif
}

std::string PlayerIdentity::GeneratePseudoUuid() {
    static std::random_device rd;
    static std::mt19937_64 rng(rd() ^ static_cast<unsigned long long>(time(nullptr)));
    std::uniform_int_distribution<int> hexDist(0, 15);

    const char* hexChars = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char& c : uuid) {
        if (c == 'x') {
            c = hexChars[hexDist(rng)];
        } else if (c == 'y') {
            c = hexChars[8 + (hexDist(rng) % 4)];
        }
    }
    return uuid;
}

void PlayerIdentity::LoadOrCreate() {
#if CANNON_DUEL_WEB_BUILD
    WebIdbfsMountAndSyncIn();
#endif
    const std::string path = SavePath();

    // Preferir API raylib (Android-friendly) e fallback fstream.
    if (FileExists(path.c_str())) {
        char* raw = LoadFileText(path.c_str());
        if (raw) {
            std::istringstream in(raw);
            std::string line1, line2;
            std::getline(in, line1);
            std::getline(in, line2);
            UnloadFileText(raw);
            // trim CR
            if (!line1.empty() && line1.back() == '\r') line1.pop_back();
            if (!line2.empty() && line2.back() == '\r') line2.pop_back();
            if (!line1.empty()) {
                id = line1;
                displayName = line2.empty() ? ("Canhoneiro#" + id.substr(0, 4)) : line2;
                DebugLogf(LOG_INFO, "IDENTITY: carregada id=%s nome=%s path=%s",
                          id.c_str(), displayName.c_str(), path.c_str());
                return;
            }
        }
    }

    id = GeneratePseudoUuid();
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    int suffix = 1000 + static_cast<int>(rng() % 9000);
    displayName = "Canhoneiro#" + std::to_string(suffix);
    Save();
    DebugLogf(LOG_INFO, "IDENTITY: NOVA id=%s nome=%s path=%s",
              id.c_str(), displayName.c_str(), path.c_str());
}

void PlayerIdentity::SetDisplayName(const std::string& name) {
    std::string clean = net_validation::SanitizeDisplayName(name);
    if (clean.empty()) return;
    displayName = clean;
    Save();
}

void PlayerIdentity::Save() const {
    const std::string path = SavePath();
    std::string body = id + "\n" + displayName + "\n";
    if (!SaveFileText(path.c_str(), body.data())) {
        std::ofstream out(path);
        out << body;
    }
#if CANNON_DUEL_WEB_BUILD
    WebIdbfsSyncOut();
#endif
}
