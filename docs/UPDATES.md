# In-game updates

Options → Check for updates displays CURRENT VERSION (compiled from the CMake
project version), LATEST VERSION, and one of: checking, new version available,
up to date, local build is newer, or a check error. Comparison is numeric across
three/four components, not alphabetical. Failed checks do not claim the player
is up to date. No network request occurs until the update screen is opened.

## Installation support

- Windows x64/x86 desktop: select INSTALL UPDATE, confirm DOWNLOAD, then wait
  for verification. Select RESTART TO UPDATE when ready. This saves preferences,
  closes normally, applies the package, and launches the new executable. It does
  not interrupt gameplay automatically. CANCEL stops a check/download; BACK can
  leave the screen while work continues. Closing without installing discards
  this session's staged download. Running from a source checkout disables
  in-place installation, including build/current and build/release.
- Portable Linux x64 desktop: the same confirmed download/restart flow is
  supported, including Steam Deck's extracted desktop ZIP. Version checks use
  `/usr/bin/curl`; installation uses `/usr/bin/python3` (standard library only,
  no pip/system package installation). Missing Python, root-owned/read-only
  directories, source checkouts, AppImage/Flatpak/Snap and system-managed
  installation paths (including Steam-managed `steamapps` directories) stay manual. Existing nested and flat ZIP layouts are
  supported. Executable permissions are restored; user data is preserved.
- macOS desktop: version checks use `/usr/bin/curl` and RELEASE NOTES opens the
  official download page. In-place replacement is not implemented for macOS.
- Android/iOS/UWP/Switch/Vita/PS5: use RELEASE NOTES (where the platform supports
  opening URLs) and the normal platform installation method. Native version
  checking and automatic installation are not implemented on these platforms.
- PCVR/Quest/Steam Frame use separate frontends/packages; this initial feature
  is in the SDL desktop player, not the VR frontend. Never install a flat-screen
  Windows ZIP over a PCVR installation. Nintendo 3DS work remains paused.

Existing 0.0.8.8 downloads do not acquire new menu code automatically. The first
release containing the updater must be installed normally.

## Safety and failure behavior

The player reads only the fixed official public GitHub repository's latest
published stable release. It sends no GitHub credentials or game/user data.
Drafts, prereleases, malformed versions, downgrades, unexpected repositories,
wrong asset names, missing SHA-256 digests and invalid asset sizes cannot become
automatic installs. HTTP errors/rate limits are reported, not interpreted as
"up to date". Checks/downloads run on a worker; cancelled/failed downloads do not
modify the installed game. TLS certificate validation stays enabled, and
download redirects are HTTPS-only to GitHub's known release hosts.

Windows uses built-in WinHTTP and an embedded PowerShell helper (no extra
runtime download). The helper is local application code, not commands fetched
from release notes. Its process uses RemoteSigned; it does not change persistent
execution policy or override Group Policy. Locked-down machines can use manual
installation. No privilege elevation, antivirus exclusions, optional runtime
installation, registry edits or security-check suppression are performed.

The downloaded ZIP's exact byte size and SHA-256 must match GitHub's HTTPS
release-asset metadata. This detects corruption; it is not a separately signed
publisher manifest. GitHub account/release security remains part of the trust
boundary. Packages without a GitHub digest are manual-only.

The helper rejects traversal, absolute/ADS/device paths, case-insensitive
duplicates, symbolic links and unsafe directory ancestors; extraction is
bounded to 20,000 entries / 8 GiB. It checks available disk space and the PE CPU
architecture (ELF on Linux). Only an explicit allowlist of package executables, libraries,
documentation, licenses and optional-runtime *files* can be staged. User
settings, saves, ROMs, BIN/PAK files, layouts and unrelated files are not copied
over or deleted. Old package files not present in the new package are retained.

Only after the requesting game process exits are files replaced, with a
same-volume original-file backup. Failure during replacement or launching the
new player rolls back changed files. A failed detached install keeps its error
receipt/backup beneath the updater's workspace/installation folder and shows
an error dialog. Abrupt power loss is not a tested transactional recovery path;
keep a leftover `sfe-update-…-rollback/original` directory for manual recovery.
Successful updates remove their own staging/backup files only. Another
concurrently running copy can prevent replacement; close other copies first.

## Validation

`starfox_update_tests` covers numeric versions, stable-release/asset policy,
hostile metadata and idle behavior. `starfox_update_tests --live-check` performs
a read-only real release check. On Windows, build `starfox_update_exit_fixture`
and run `python tests/test_app_update.py`: tiny synthetic ZIPs exercise safe
staging, hash/architecture/path/link rejection, preservation, restart and
rollback. Tests use build/validation/active, not real installs or release ZIPs.

`python3 tests/test_linux_app_update.py` covers Linux archive policy on any
host, plus actual Linux parent-process waiting, permissions, restart and
rollback on native Linux. Set STARFOX_UPDATE_TEST_FIXTURE to the tiny native
`starfox_update_exit_fixture` executable. The PR's updater-only CI compiles the
actual service on Windows/Linux/macOS without private assets or new releases.
Linux failed-install receipts remain in the updater cache; the next update
screen reports failure, and the old player is reopened after a successful
rollback where possible. A desktop error dialog is shown when kdialog/zenity
is installed; neither is required for installation.
