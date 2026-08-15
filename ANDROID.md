# Port para Android

Build 100% via linha de comando, sem Gradle e sem Android Studio.

## Pré-requisitos

- Android SDK (`ANDROID_HOME`), com `build-tools` e `platforms` instalados.
- Android NDK (`ANDROID_NDK_HOME`), instalado via `sdkmanager "ndk;<versão>"`.
- JDK (usado pelo `apksigner`/`keytool`).
- `git` (pra clonar o código-fonte do raylib na primeira execução).

```bash
export ANDROID_HOME=$HOME/.android_SDK
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/27.0.12077973
export PATH=$PATH:$ANDROID_HOME/platform-tools
```

## Como buildar

Debug e release **compartilham `android/build/`** — mesmo princípio do desktop.
Para trocar debug ↔ release, rode `./run_cmake_android.sh` de novo com o outro
modo.

```bash
./run_cmake_android.sh debug    # ou: ./run_cmake_android.sh release
./build_android.sh
```

O APK final fica sempre em `android/build/CannonDuel.apk`.

Debug e release são assinados com a **mesma chave** (`android/cannon-duel.keystore`),
pra poder instalar um por cima do outro sem desinstalar. A chave e
`android/keystore.properties` estão no `.gitignore` — faça backup; se perder
ou gerar outra, o Android recusa o update.

## Instalando no celular

Com o celular conectado por USB e depuração USB ativada:

```bash
$ANDROID_HOME/platform-tools/adb install -r android/build/CannonDuel.apk
```

## Multiplayer online no Android

Edite `src/net/SupabaseConfig.h` com a URL e a chave do seu projeto
Supabase antes de buildar (o mesmo arquivo vale pra PC e Android).
