#!/usr/bin/env python3
"""Check public hotfix names without feeding them to restricted native versions."""
import argparse
from pathlib import Path
import re


def check(root: Path, tag: str | None = None) -> str:
    def read(name: str) -> str:
        return (root / name).read_text(encoding="utf-8")

    def require(condition: bool, message: str) -> None:
        if not condition:
            raise ValueError(message)

    version_file = read("cmake/ReleaseVersion.cmake")
    match = re.search(r'set\(STARFOX_RELEASE_VERSION "([0-9.]+)"\)', version_file)
    require(match is not None, "Missing public release version")
    version = match[1]
    parts = version.split(".")
    require(3 <= len(parts) <= 5 and all(re.fullmatch(r"0|[1-9][0-9]*", p) for p in parts),
            "Public release version must have three to five numeric components")
    if tag is not None:
        require(tag == "v" + version, "Release tag and compiled public version differ")
    require('"@STARFOX_RELEASE_VERSION@"' in read("src/app/update_build.hpp.in"),
            "Updater must display the full public release version")
    require(f"releases/tag/v{version}" in read("README.md"), "README download is stale")
    require(read(f"docs/RELEASE-{version}.md").startswith(f"# Star Fox Enhanced {version}\n"),
            "Release notes heading is stale")

    workflow = read(".github/workflows/portable-builds.yml")
    package_versions = re.findall(r"StarFox(?:Enhanced|AssetBuilder)-([0-9.]+)-", workflow)
    require(package_versions and set(package_versions) == {version}, "Workflow package names disagree")
    require(f"StarFoxEnhanced-{version}-xbox-uwp-x64.appx" in read("tools/build_xbox_uwp.ps1"),
            "UWP output filename is stale")
    for path in ("platform/android/app/build.gradle", "platform/quest/build.gradle"):
        gradle = read(path)
        require(f"versionName '{version}'" in gradle and "versionCode 21" in gradle,
                f"Incorrect Android package version: {path}")
    require(workflow.count(f"versionCode='21' versionName='{version}'") == 2,
            "Android/Quest CI must validate native and public versions")
    require('MACOSX_BUNDLE_BUNDLE_VERSION "0.0.811"' in read("CMakeLists.txt")
            and '.CFBundleVersion == "0.0.811"' in workflow,
            "Apple build version differs from package validation")
    require('set(STARFOX_UWP_BUILD_REVISION "11"' in read("CMakeLists.txt")
            and "[int]$BuildRevision = 11" in read("tools/build_xbox_uwp.ps1"),
            "UWP build revisions disagree")
    require('set(STARFOX_VITA_APP_VERSION "00.91"' in read("cmake/VitaPackage.cmake"),
            "Vita build version is stale")
    require('set(STARFOX_PS5_CONTENT_VERSION "01.000.003")' in read("cmake/PS5.cmake"),
            "PS5 build version is stale")
    return version


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", help="Also verify the release tag, including its v prefix")
    args = parser.parse_args()
    print("Release metadata verified:", check(Path(__file__).resolve().parents[1], args.tag))
