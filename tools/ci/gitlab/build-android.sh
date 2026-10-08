#!/usr/bin/env bash
set -euo pipefail

target="${1:?expected android or quest}"
cd "${CI_PROJECT_DIR:?CI_PROJECT_DIR is required}"
version="${CI_COMMIT_TAG:-${CI_RELEASE_TAG:-v0.0.8}}"
version="${version#v}"

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates curl git unzip zip python3 cmake ninja-build openssl

export ANDROID_HOME="$CI_PROJECT_DIR/.android-sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
mkdir -p "$ANDROID_HOME/cmdline-tools" release-out build
sdk_zip="$CI_PROJECT_DIR/.android-commandline-tools.zip"
curl --fail --location --retry 3 \
    'https://dl.google.com/android/repository/commandlinetools-linux-15859902_latest.zip' \
    --output "$sdk_zip"
echo "4e4c464f145a7512b57d088ac6c278c03c9eea610886b35a5e0804e74eedf583  $sdk_zip" \
    | sha256sum --check --status
unzip -q "$sdk_zip" -d "$ANDROID_HOME/cmdline-tools"
mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest"
sdkmanager="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"
yes | "$sdkmanager" --licenses >/dev/null || true
"$sdkmanager" --install \
    'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' \
    'ndk;28.2.13676358' 'cmake;3.22.1'

export ANDROID_RELEASE_KEYSTORE_PATH="$CI_PROJECT_DIR/.android-release.p12"
umask 077
validation_signing=0
if [[ -n "${ANDROID_RELEASE_KEYSTORE:-}" && -n "${ANDROID_RELEASE_PASSWORD:-}" ]]; then
    printf '%s' "$ANDROID_RELEASE_KEYSTORE" | base64 --decode \
        > "$ANDROID_RELEASE_KEYSTORE_PATH"
else
    if [[ "${CI_PUBLISH_RELEASE:-false}" == true || -n "${CI_COMMIT_TAG:-}" ]]; then
        echo 'Official Android/Quest release signing variables are missing.' >&2
        exit 1
    fi
    export ANDROID_RELEASE_PASSWORD="$(openssl rand -hex 24)"
    keytool -genkeypair -noprompt -storetype PKCS12 \
        -keystore "$ANDROID_RELEASE_KEYSTORE_PATH" \
        -storepass "$ANDROID_RELEASE_PASSWORD" -keypass "$ANDROID_RELEASE_PASSWORD" \
        -alias starfox-enhanced -keyalg RSA -keysize 3072 -validity 2 \
        -dname 'CN=Star Fox Enhanced private GitLab validation build'
    validation_signing=1
fi

case "$target" in
android)
    bash tools/build_android.sh "$CI_PROJECT_DIR" debug
    apk=dist/StarFoxEnhanced-android-arm64.apk
    python3 tools/check_android_package.py "$apk" --source-root "$CI_PROJECT_DIR"
    "$ANDROID_HOME/build-tools/35.0.0/aapt" dump badging "$apk"
    unzip -l "$apk" | grep -q 'lib/arm64-v8a/libmain.so'
    unzip -l "$apk" | grep -q 'lib/arm64-v8a/libSDL3.so'
    cp "$apk" "release-out/StarFoxEnhanced-${version}-android-arm64.apk"
    ;;
quest)
    (
        while true; do
            echo "--- Quest runner resources $(date -u +%FT%TZ) ---"
            free -m
            df -h "$CI_PROJECT_DIR"
            ps -eo pid,ppid,rss,vsz,comm --sort=-rss | sed -n '1,12p'
            sleep 20
        done
    ) &
    monitor_pid=$!
    trap 'kill "$monitor_pid" 2>/dev/null || true' EXIT
    bash tools/build_android.sh "$CI_PROJECT_DIR" release quest
    apk=dist/StarFoxEnhanced-quest-arm64.apk
    python3 tools/check_quest_package.py "$apk" --source-root .
    "$ANDROID_HOME/build-tools/35.0.0/aapt" dump badging "$apk" \
        > "$CI_PROJECT_DIR/build/quest-badging.txt"
    grep -Fq "package: name='com.starfox.enhanced.quest'" \
        "$CI_PROJECT_DIR/build/quest-badging.txt"
    mkdir -p quest-package/licenses/fonts
    cp "$apk" quest-package/
    cp tools/package/QUEST-START-HERE.txt quest-package/START-HERE.txt
    cp THIRD_PARTY_NOTICES.md CREDITS.md LICENSE-XBRZ.txt quest-package/
    cp assets/fonts/README.md assets/fonts/misaki.txt quest-package/licenses/fonts/
    (cd quest-package && zip -9 -r "../release-out/StarFoxEnhanced-${version}-quest-arm64.zip" .)
    ;;
*)
    echo "Unknown target: $target" >&2
    exit 2
    ;;
esac

"$ANDROID_HOME/build-tools/35.0.0/apksigner" verify --print-certs "$apk" \
    > "$CI_PROJECT_DIR/build/signing-certificate.txt"
if [[ "$validation_signing" != 1 ]]; then
    grep -Fqi '50bcb41f7a02c2e9bb0f68a74b1f6e14f7317fedc69d2c6fd70a165f87800fc0' \
        "$CI_PROJECT_DIR/build/signing-certificate.txt"
fi
