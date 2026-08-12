#pragma once
#include "../Platform.h"

// Símbolos gerados por cmake/EmbedCaCert.cmake (xxd -i cacert.pem).
#if CANNON_DUEL_HAS_EMBEDDED_CA
extern "C" {
extern unsigned char cacert_pem[];
extern unsigned int cacert_pem_len;
}
#endif
