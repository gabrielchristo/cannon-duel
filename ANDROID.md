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

```bash
./build_android.sh debug    # -> android/build/
./build_android.sh release  # -> android/build_release/
```

O APK final fica em `android/build/CannonDuel.apk` (ou
`android/build_release/CannonDuel.apk` pra release).

## Instalando no celular

Com o celular conectado por USB e depuração USB ativada:

```bash
$ANDROID_HOME/platform-tools/adb install -r android/build/CannonDuel.apk
```

## Multiplayer online no Android

Edite `src/net/SupabaseConfig.h` com a URL e a chave do seu projeto
Supabase antes de buildar (o mesmo arquivo vale pra PC e Android).
