Este arquivo precisa ser substituído pelo certificado CA de verdade.

Rode isso na raiz do projeto:

  curl -o assets/certs/cacert.pem https://curl.se/ca/cacert.pem

Estratégia TLS por plataforma (implementada em src/net/TlsCaBundle.cpp):

  Desktop (PC): lê assets/certs/cacert.pem em disco se existir; senão
                usa o repositório de CAs do sistema operacional.

  Android/iOS:  cacert.pem é EMBUTIDO no binário pelo CMake
                (cmake/EmbedCaCert.cmake + xxd -i) e passado ao curl via
                CURLOPT_CAINFO_BLOB — curl estático nessas plataformas não
                tem CAs do SO. Mesmo módulo CMake serve pros dois.

NÃO use LoadFileData() para o cacert no mobile — ftell() do raylib retorna
0 bytes em assets empacotados.
