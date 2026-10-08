#!/usr/bin/env bash
set -euo pipefail

cd "${CI_PROJECT_DIR:?CI_PROJECT_DIR is required}"
tag="${CI_COMMIT_TAG:-${CI_RELEASE_TAG:-}}"
if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Refusing invalid release tag: $tag" >&2
    exit 2
fi
if ! git rev-parse --verify --quiet "refs/tags/$tag^{commit}" >/dev/null; then
    echo "Release tag $tag does not exist in this repository" >&2
    exit 1
fi
version="${tag#v}"
notes="docs/RELEASE-${version}.md"
if [[ ! -f "$notes" ]]; then
    echo "Release notes are missing: $notes" >&2
    exit 1
fi

build_set="${CI_BUILD_SET:-all}"
case "$build_set" in
all)
    expected=(windows-x64 windows-x86 windows-pcvr linux-x64
        macos-universal ios-arm64-unsigned android-arm64 quest-arm64
        switch-homebrew vita)
    ;;
desktop)
    expected=(windows-x64 windows-x86 windows-pcvr linux-x64)
    ;;
linux)
    expected=(linux-x64)
    ;;
windows-x64)
    expected=(windows-x64)
    ;;
windows-x86)
    expected=(windows-x86)
    ;;
pcvr)
    expected=(windows-pcvr)
    ;;
mobile)
    expected=(ios-arm64-unsigned android-arm64)
    ;;
android)
    expected=(android-arm64)
    ;;
apple)
    expected=(macos-universal ios-arm64-unsigned)
    ;;
quest)
    expected=(quest-arm64)
    ;;
homebrew)
    expected=(switch-homebrew vita)
    ;;
switch)
    expected=(switch-homebrew)
    ;;
vita)
    expected=(vita)
    ;;
*)
    echo "Nothing publishable was selected (CI_BUILD_SET=$build_set)" >&2
    exit 2
    ;;
esac

for platform in "${expected[@]}"; do
    if ! find release-out -maxdepth 1 -type f \
        -name "StarFoxEnhanced-${version}-${platform}.*" -print -quit \
        | grep -q .; then
        echo "Missing verified $platform package; release not published" >&2
        exit 1
    fi
done

mapfile -d '' packages < <(find release-out -maxdepth 1 -type f \
    \( -name '*.zip' -o -name '*.apk' \) -print0 | sort -z)
if (( ${#packages[@]} == 0 )); then
    echo 'No release packages were downloaded from build jobs' >&2
    exit 1
fi

(
    cd release-out
    for package in "${packages[@]}"; do
        sha256sum -- "$(basename "$package")"
    done > SHA256SUMS.txt
)
packages+=(release-out/SHA256SUMS.txt)

export GLAB_ENABLE_CI_AUTOLOGIN=true
release_name="Star Fox Enhanced ${version}"
if [[ "$build_set" != all ]]; then
    release_name+=" (partial)"
    partial_notes="release-out/RELEASE-PARTIAL.md"
    {
        printf '**Partial GitLab release.** Verified packages in this upload: %s.\n\n' "${expected[*]}"
        printf 'macOS/iOS builds are unavailable without a Mac runner; Android/Quest upgrade-compatible APKs require the original signing key.\n\n'
        printf 'These binaries were built from CI commit `%s`; the existing `%s` tag remains at its original source commit.\n\n' "${CI_COMMIT_SHA:?CI_COMMIT_SHA is required}" "$tag"
        cat "$notes"
    } > "$partial_notes"
    notes="$partial_notes"
fi
glab release create "$tag" "${packages[@]}" \
    --name "$release_name" \
    --notes-file "$notes" \
    --use-package-registry --package-name release-assets
echo "Published ${#packages[@]} verified assets to $tag"
