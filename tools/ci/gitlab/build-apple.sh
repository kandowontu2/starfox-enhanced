#!/usr/bin/env bash
set -euo pipefail

target="${1:?expected macos or ios}"
cd "${CI_PROJECT_DIR:?CI_PROJECT_DIR is required}"
mkdir -p release-out
version="${CI_COMMIT_TAG:-${CI_RELEASE_TAG:-v0.0.8}}"
version="${version#v}"

case "$target" in
macos)
    bash tools/build_apple.sh "$CI_PROJECT_DIR" macos
    app=build/macos-universal/Release/StarFoxEnhanced.app
    binary="$app/Contents/MacOS/StarFoxEnhanced"
    test -d "$app"
    test -x "$binary"
    plutil -lint "$app/Contents/Info.plist"
    archs="$(lipo -archs "$binary")"
    [[ " $archs " == *" arm64 "* ]]
    [[ " $archs " == *" x86_64 "* ]]
    package="StarFoxEnhanced-${version}-macos-universal"
    mkdir -p "$package"
    cp -R "$app" "$package/"
    cp build/macos-universal/Release/starfox_asset_builder "$package/"
    cp platform/mobile/ASSET_BUILDER.md "$package/ASSET_BUILDER.md"
    cp README.md CREDITS.md THIRD_PARTY_NOTICES.md "$package/"
    ditto -c -k --sequesterRsrc --keepParent "$package" "release-out/$package.zip"
    ;;
ios)
    bash tools/build_apple.sh "$CI_PROJECT_DIR" ios
    app=build/ios/Release-iphoneos/StarFoxEnhanced.app
    binary="$app/StarFoxEnhanced"
    test -d "$app"
    test -f "$binary"
    plutil -lint "$app/Info.plist"
    plutil -convert json -o - "$app/Info.plist" \
        | jq -e '.CADisableMinimumFrameDurationOnPhone == true' >/dev/null
    [[ " $(lipo -archs "$binary") " == *" arm64 "* ]]
    package="StarFoxEnhanced-${version}-ios-arm64-unsigned"
    mkdir -p "$package/Payload"
    cp -R "$app" "$package/Payload/"
    cp platform/apple/IOS_INSTALL.md "$package/README.md"
    ditto -c -k --sequesterRsrc --keepParent "$package" "release-out/$package.zip"
    ;;
*)
    echo "Unknown target: $target" >&2
    exit 2
    ;;
esac
