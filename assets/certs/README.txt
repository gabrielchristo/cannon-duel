Este arquivo precisa ser substituído pelo certificado CA de verdade.

Rode isso na raiz do projeto:

  curl -o assets/certs/cacert.pem https://curl.se/ca/cacert.pem

(ou baixe manualmente https://curl.se/ca/cacert.pem e salve como
assets/certs/cacert.pem)

Esse arquivo é o pacote de certificados raiz que o curl/Mozilla mantém
publicamente pra builds que não têm acesso ao repositório de confiança do
sistema operacional (é exatamente o nosso caso na build Android — o curl
que compilamos do zero com mbedTLS não tem nenhum certificado embutido).
Sem esse arquivo em assets/certs/cacert.pem, toda requisição HTTPS do
multiplayer online falha silenciosamente no Android.

No desktop isso não é estritamente necessário (o curl do sistema já
resolve sozinho), mas o código aponta pra esse mesmo arquivo nas duas
plataformas por consistência — então baixe de qualquer forma.
