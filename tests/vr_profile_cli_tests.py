#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def run(executable, *arguments):
    return subprocess.run(
        [executable, *arguments], capture_output=True, text=True, timeout=5
    )


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def check_pcvr(executable):
    # PCVR and Quest never grew the profiling options: they stay unknown.
    help_result = run(executable, "--help")
    require(help_result.returncode == 0, "PCVR help did not exit successfully")
    require("--profile" not in help_result.stdout, "PCVR help lists profiling options")
    for arguments in (("--profile-csv", "unused.csv"), ("--profile-frames", "1")):
        result = run(executable, *arguments)
        require(result.returncode == 2 and "Unknown or incomplete option" in result.stderr,
                "PCVR accepted a Steam Frame profiling option")
    print("PCVR keeps its original command line (no profiling options)")


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--steam-frame"):
        raise RuntimeError("expected path to starfox_pcvr, or path to starfox_steamframe --steam-frame")
    executable = sys.argv[1]
    if len(sys.argv) == 2:
        check_pcvr(executable)
        return
    help_result = run(executable, "--help")
    require(help_result.returncode == 0, "Steam Frame help did not exit successfully")
    require("--profile-csv FILE" in help_result.stdout
            and "default 120" in help_result.stdout
            and "1..1000000" in help_result.stdout,
            "Steam Frame help does not explain bounded CSV profiling")

    invalid_limit = run(executable, "--profile-frames", "0")
    require(invalid_limit.returncode == 2
            and "1..1000000" in invalid_limit.stderr,
            "zero profile frame limit was not rejected before runtime startup")
    missing_path = run(executable, "--profile-frames", "1")
    require(missing_path.returncode == 2
            and "requires --profile-csv" in missing_path.stderr,
            "profile frame limit was accepted without an output path")
    oversized_limit = run(executable, "--profile-csv", "unused.csv",
                          "--profile-frames", "1000001")
    require(oversized_limit.returncode == 2
            and "1..1000000" in oversized_limit.stderr,
            "frame limit above the documented maximum was accepted")
    with tempfile.TemporaryDirectory() as temporary:
        missing_bundle = str(Path(temporary) / "missing-assets.bin")
        long_capture = run(executable, "--bundle", missing_bundle,
                           "--profile-csv", str(Path(temporary) / "profile.csv"),
                           "--profile-frames", "162000")
        require(long_capture.returncode == 2
                and "Missing asset bundle:" in long_capture.stderr
                and "1..1000000" not in long_capture.stderr,
                "30-minute 90 Hz frame limit was rejected before bundle validation")
    print("VR profiling CLI: help, bounded frame limit, and output-path checks passed")


if __name__ == "__main__":
    main()
