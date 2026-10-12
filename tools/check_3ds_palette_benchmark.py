#!/usr/bin/env python3
"""Compare real warm BG2 atlas recolouring, without rebuilding either graph.

The baseline implementation is compiled against the same current headers and
linked before the existing archives. No ROM, player or whole source copy is
needed. These are host component timings, not native-console FPS acceptance.
"""
import argparse
import hashlib
import json
import re
import shlex
import statistics
import subprocess
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(arguments, build):
    return subprocess.run(arguments, cwd=build, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--baseline-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, default=16)
    args = parser.parse_args()
    if not 8 <= args.pairs <= 32:
        parser.error("use 8--32 alternating-order pairs")
    build = args.build.resolve()
    baseline_source = args.baseline_source.resolve()
    output = args.output.resolve()
    baseline_object = build / "palette-baseline.o"
    baseline_exe = build / "palette-baseline"
    baseline_dependencies = build / "palette-baseline-link.d"
    candidate_exe = build / "starfox_3ds_bg2_tile_tests"
    if any(path.exists() for path in (output, baseline_object, baseline_exe, baseline_dependencies)):
        raise RuntimeError("Preserve existing baseline and benchmark receipts")
    entries = json.loads((build / "compile_commands.json").read_text())
    entries = [entry for entry in entries if
               Path(entry["file"]).name == "pica_bg2_tiles.cpp"]
    if len(entries) != 1:
        raise RuntimeError("Ambiguous atlas compiler command")
    command = entries[0].get("arguments") or shlex.split(entries[0]["command"])
    candidate_source = Path(entries[0]["file"]).resolve()
    source_hashes = {"baseline": sha256(baseline_source),
                     "candidate": sha256(candidate_source)}
    compile_args = []
    at = 0
    replacements = 0
    while at < len(command):
        arg = command[at]
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            at += 2
            continue
        if arg in ("-MD", "-MMD"):
            at += 1
            continue
        if arg == entries[0]["file"]:
            arg = str(baseline_source)
            replacements += 1
        compile_args.append(arg)
        at += 1
    if replacements != 1:
        raise RuntimeError("Baseline source replacement was not exact")
    compile_args += ["-o", str(baseline_object)]
    run(compile_args, build)
    commands = run(["ninja", "-t", "commands", candidate_exe.name], build)
    link_args = shlex.split(commands.splitlines()[-1])
    # CMake's POSIX link wrapper is ': && COMPILER ... && :'. Never execute
    # that shell wrapper or pass a string-built command to another shell.
    if link_args[:2] == [":", "&&"]:
        link_args = link_args[2:]
    if link_args[-2:] == ["&&", ":"]:
        link_args = link_args[:-2]
    if any(arg in ("&&", ";", "|") for arg in link_args):
        raise RuntimeError("Unexpected shell operation in the link recipe")
    # Recent CMake versions add a linker depfile to the candidate recipe.
    # Redirect that metadata too; the baseline must not overwrite link.d in
    # the existing candidate graph even though its executable stays unchanged.
    link_args = ["-Wl,--dependency-file=" + str(baseline_dependencies)
                 if arg.startswith("-Wl,--dependency-file=") else arg
                 for arg in link_args]
    out_at = link_args.index("-o")
    if Path(link_args[out_at + 1]).name != candidate_exe.name:
        raise RuntimeError("Unexpected candidate link output")
    link_args[out_at + 1] = str(baseline_exe)
    link_args.insert(out_at, str(baseline_object))
    run(link_args, build)
    executable_hashes = {"baseline": sha256(baseline_exe),
                         "candidate": sha256(candidate_exe)}
    pattern = re.compile(r"iterations=4096 nanoseconds=(\d+) digest=([0-9a-f]+)")
    samples = {"baseline": [], "candidate": []}
    digest = None
    # Warm each executable before measuring. Every measured invocation keeps
    # the same 4096 source fade updates; alternating order avoids warm-first bias.
    for name, exe in (("baseline", baseline_exe), ("candidate", candidate_exe)):
        run([str(exe), "--palette-benchmark"], build)
    for pair in range(args.pairs):
        order = ("baseline", "candidate") if pair % 2 == 0 else ("candidate", "baseline")
        for name in order:
            exe = baseline_exe if name == "baseline" else candidate_exe
            measured = run([str(exe), "--palette-benchmark"], build)
            match = pattern.search(measured)
            if not match:
                raise RuntimeError("Missing actual completed benchmark result")
            if digest is None:
                digest = match[2]
            if match[2] != digest:
                raise RuntimeError("Baseline/candidate atlas observation differs")
            samples[name].append(int(match[1]))
    if source_hashes != {"baseline": sha256(baseline_source), "candidate": sha256(candidate_source)}:
        raise RuntimeError("Source changed during the comparison")
    if executable_hashes != {"baseline": sha256(baseline_exe), "candidate": sha256(candidate_exe)}:
        raise RuntimeError("Executable changed during the comparison")
    baseline_median = statistics.median(samples["baseline"])
    candidate_median = statistics.median(samples["candidate"])
    receipt = {"scope": "host atlas recolouring, not ARM or whole-game FPS",
               "pairs": args.pairs, "updates_per_invocation": 4096,
               "source_sha256": source_hashes, "executable_sha256": executable_hashes,
               "baseline_compile_arguments": compile_args,
               "baseline_link_arguments": link_args, "observation_digest": digest,
               "nanoseconds": samples, "median_speed_ratio": baseline_median / candidate_median,
               "native_console_accepted": False}
    output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Actual atlas recolouring: {baseline_median / candidate_median:.3f}x median "
          f"over {args.pairs} pairs; unchanged observation {digest}; NOT console FPS")


if __name__ == "__main__":
    main()
