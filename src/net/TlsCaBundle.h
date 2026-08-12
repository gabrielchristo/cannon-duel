#pragma once

#include <curl/curl.h>
#include <vector>
#include <string>

// Pacote de CAs raiz para verificação TLS do curl/Supabase.
// Estratégia por plataforma (ver TlsCaBundle.cpp):
//   Mobile (Android/iOS): PEM embutido no binário → CURLOPT_CAINFO_BLOB
//   Desktop:              assets/certs/cacert.pem em disco, ou CAs do SO
struct TlsCaBundle {
    bool loaded = false;
    std::vector<unsigned char> pem;
    std::string filePath; // fallback desktop quando libcurl < 7.77
#if LIBCURL_VERSION_NUM >= 0x074D00
    curl_blob blob{};
#endif
};

TlsCaBundle LoadTlsCaBundle();
void ApplyTlsCaToCurl(CURL* curl, const TlsCaBundle& ca);
void ProbeTlsCaBundle();
