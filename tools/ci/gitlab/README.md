# GitLab release builds

`.gitlab-ci.yml` replaces the portable-builds GitHub Actions workflow. It
builds Windows x64/x86, PCVR, Linux, macOS, iOS, Android, Quest, Switch and
Vita on separate clean runners and stores packages as GitLab job artifacts.
The Xbox UWP job requires a Windows runner with the UWP workload and VCLibs;
it is manual until such a runner is registered with tag
`starfox-windows-uwp`. No ROMs or private asset bundles are packaged.

For 0.0.8, run a pipeline on the CI branch and select the `desktop` build-set
input first. Individual choices (`linux`, `windows-x64`, `windows-x86`, `pcvr`,
`android`, `quest`, `switch`, and `vita`) are available for isolated retries.
`homebrew`, `apple`, `mobile`, and `all` select groups; the Apple jobs require
a macOS runner. Leave the release-tag input at `v0.0.8`; setting
publish-release to `true` publishes only the selected verified packages and a
SHA256 manifest to that tag's private GitLab release. The release script
refuses missing packages for the selected build set, and the default `smoke`
pipeline only validates Linux without publishing. Tagged pipelines likewise
do not publish unless explicitly requested.

For official Android/Quest release packages, set masked, protected GitLab
CI/CD variables `ANDROID_RELEASE_KEYSTORE` (base64 PKCS#12) and
`ANDROID_RELEASE_PASSWORD` (password for that keystore). They must be the
original release signing credentials; a temporary validation key is generated
only for non-publishing pipelines. If the original key cannot be recovered,
Android/Quest upgrades will require a separate signing-key migration plan.

This project's GitLab account currently has neither a Mac runner nor access
to hosted macOS runners, so macOS and iOS packages cannot be rebuilt here.
The original Android/Quest signing key is also unavailable, so their CI jobs
can validate with a temporary key but cannot publish upgrade-compatible APKs.
Any 0.0.8 release under these conditions must be labeled partial. Build jobs
do not publish a release on their own. A new version needs a matching
`docs/RELEASE-X.Y.Z.md` file and `vX.Y.Z` tag.
