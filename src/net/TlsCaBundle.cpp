#include "TlsCaBundle.h"
#include "CaCertEmbedded.h"
#include "../AssetPath.h"
#include "../DebugLog.h"
#include "../Platform.h"
#include <cstdio>
#include <cstring>

namespace {

const char* PlatformLabel() {
#if CANNON_DUEL_ANDROID_BUILD
    return "Android";
#elif CANNON_DUEL_IOS_BUILD
    return "iOS";
#else
    return "Desktop";
#endif
}

bool LoadPemFromFile(const std::string& path, TlsCaBundle& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    out.pem.resize(static_cast<size_t>(size));
    size_t read = fread(out.pem.data(), 1, out.pem.size(), f);
    fclose(f);

    if (read != out.pem.size()) {
        out.pem.clear();
        return false;
    }

    out.filePath = path;
    out.loaded = true;
    return true;
}

#if CANNON_DUEL_HAS_EMBEDDED_CA
bool LoadPemFromEmbedded(TlsCaBundle& out) {
    if (cacert_pem_len == 0) return false;
    out.pem.assign(cacert_pem, cacert_pem + cacert_pem_len);
    out.loaded = true;
    return true;
}
#endif

void FinalizeBlob(TlsCaBundle& ca) {
#if LIBCURL_VERSION_NUM >= 0x074D00
    if (!ca.pem.empty()) {
        ca.blob.data = ca.pem.data();
        ca.blob.len = ca.pem.size();
        ca.blob.flags = CURL_BLOB_NOCOPY;
    }
#else
    (void)ca;
#endif
}

} // namespace

TlsCaBundle LoadTlsCaBundle() {
    static TlsCaBundle cached;
    static bool attempted = false;
    if (attempted) return cached;
    attempted = true;

    DebugLogf(LOG_INFO, "TLS: carregando CA (%s)", PlatformLabel());

#if CANNON_DUEL_HAS_EMBEDDED_CA
    if (LoadPemFromEmbedded(cached)) {
        FinalizeBlob(cached);
        DebugLogf(LOG_INFO, "TLS: CA embutido OK (%zu bytes)", cached.pem.size());
        return cached;
    }
    DebugLogf(LOG_WARNING, "TLS: CA embutido ausente ou vazio — rebuild com EmbedCaCert.cmake");
#elif CANNON_DUEL_DESKTOP_BUILD
    std::string path = AssetPath("certs/cacert.pem");
    if (LoadPemFromFile(path, cached)) {
        FinalizeBlob(cached);
        DebugLogf(LOG_INFO, "TLS: CA em %s (%zu bytes)", path.c_str(), cached.pem.size());
        return cached;
    }
    DebugLogf(LOG_INFO, "TLS: %s não encontrado — usando CAs do sistema", path.c_str());
#else
    // Mobile sem embed (build incompleto) — tenta disco só no desktop;
    // em mobile fopen() não lê bundle/app assets de qualquer forma.
    DebugLogf(LOG_WARNING, "TLS: mobile sem CANNON_DUEL_HAS_EMBEDDED_CA — HTTPS vai falhar");
#endif

    return cached;
}

void ApplyTlsCaToCurl(CURL* curl, const TlsCaBundle& ca) {
    if (!curl || !ca.loaded) return;

#if LIBCURL_VERSION_NUM >= 0x074D00
    if (!ca.pem.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca.blob);
        return;
    }
#endif

    if (!ca.filePath.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca.filePath.c_str());
    }
}

void ProbeTlsCaBundle() {
    TlsCaBundle ca = LoadTlsCaBundle();
    if (CANNON_DUEL_MOBILE_BUILD && !ca.loaded) {
        DebugLogf(LOG_WARNING, "TLS: NENHUM CA em %s — curl 60 provável", PlatformLabel());
    }
}
