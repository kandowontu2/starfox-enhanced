"""Source-only SDK check. Compilation is not runtime/ABI/optical acceptance."""
import argparse
import copy
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
NAMES = set("tiles reduce query mapping domains frames optical optical_stream optical_schedule jets jets_stream roots_clear witness folds guide colour compose publish publish_diagnostics admission_clear capture history lobes paths curved_paths".split())
FLAGS = ["-std=metal3.0", "-O3", "-Werror", "-fno-fast-math", "-ffp-contract=off"]
GIB = 1 << 30


class Deferred(RuntimeError):
    """A prelaunch resource refusal, not a failed shader compiler."""


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def qualify(manifest):
    rows = manifest["Shaders"]
    if len(rows) != 25 or {r["Name"] for r in rows} != NAMES:
        raise RuntimeError("Complete25 unique shader programs required")
    for row in rows:
        if row["File"] != f"shaders/{row['Name']}.metal":
            raise RuntimeError("Unexpected or escaping source path")
        path = ROOT / row["File"]
        if digest(path).lower() != row["Sha256"].lower() or path.stat().st_size != row["Bytes"]:
            raise RuntimeError("Held complete translated source changed")
        code = path.read_text(encoding="utf-8")
        if not re.search(r"kernel\s+void\s+" + re.escape(row["Entry"]) + r"\(", code):
            raise RuntimeError("Complete kernel entry missing")
        if re.search(r'#include\s+"', code):
            raise RuntimeError("Unpinned local compiler include")
    return rows


def controls(manifest):
    qualify(manifest)
    bad = []
    shortened = copy.deepcopy(manifest)
    shortened["Shaders"].pop()
    bad.append(shortened)
    repeated = copy.deepcopy(manifest)
    repeated["Shaders"][-1] = copy.deepcopy(repeated["Shaders"][0])
    bad.append(repeated)
    for key, value in (("File", "../private.metal"), ("Sha256", "0" * 64), ("Entry", "absent_kernel"), ("Bytes", 1)):
        changed = copy.deepcopy(manifest)
        changed["Shaders"][0][key] = value
        bad.append(changed)
    for item in bad:
        try:
            qualify(item)
        except RuntimeError:
            continue
        raise RuntimeError("Altered/incomplete source control was accepted")
    owner = {"pid": 42, "birth": [1, 2], "path": "/actual/compiler"}
    if not same_identity(owner, dict(owner)) or same_identity(owner, None):
        raise RuntimeError("Native identity positive/null control failed")
    for key, value in (("pid", 43), ("birth", [1, 3]), ("path", "/other/compiler")):
        changed = dict(owner)
        changed[key] = value
        if same_identity(owner, changed):
            raise RuntimeError("Changed native identity accepted")
    print("PASS: complete25 actual source pins; missing/duplicate/escaped/changed bytes/entry/size rejected. No SDK compiler launched.")


class BsdInfo(ctypes.Structure):
    # Public Darwin proc_bsdinfo ABI: native microsecond birth, not ps seconds.
    _fields_ = [("prefix", ctypes.c_uint32 * 12), ("comm", ctypes.c_char * 16),
                ("name", ctypes.c_char * 32), ("suffix", ctypes.c_uint32 * 6),
                ("seconds", ctypes.c_uint64), ("microseconds", ctypes.c_uint64)]


class TaskInfo(ctypes.Structure):
    _fields_ = [("virtual", ctypes.c_uint64), ("resident", ctypes.c_uint64),
                ("counters", ctypes.c_uint64 * 4), ("remaining", ctypes.c_int32 * 12)]


def native(pid, library):
    bsd, task = BsdInfo(), TaskInfo()
    if library.proc_pidinfo(pid, 3, 0, ctypes.byref(bsd), ctypes.sizeof(bsd)) != ctypes.sizeof(bsd):
        return None
    if library.proc_pidinfo(pid, 4, 0, ctypes.byref(task), ctypes.sizeof(task)) != ctypes.sizeof(task):
        return None
    path = ctypes.create_string_buffer(4096)
    if library.proc_pidpath(pid, path, len(path)) <= 0:
        return None
    return {"pid": pid, "parent": int(bsd.prefix[4]), "birth": [int(bsd.seconds), int(bsd.microseconds)],
            "path": os.path.realpath(os.fsdecode(path.value)), "resident": int(task.resident), "virtual": int(task.virtual)}


def memory():
    raw = subprocess.check_output(["/usr/bin/vm_stat"], text=True)
    page = int(re.search(r"page size of (\d+) bytes", raw).group(1))
    values = {name: int(count) for name, count in re.findall(r"^([^:\n]+):\s*(\d+)\.", raw, re.M)}
    # Reclaimable physical pages on macOS, without double-counting purgeable.
    available = sum(values[k] for k in ("Pages free", "Pages inactive", "Pages speculative")) * page
    swap = subprocess.check_output(["/usr/sbin/sysctl", "-n", "vm.swapusage"], text=True)
    free, unit = re.search(r"free\s*=\s*([0-9.]+)([KMG])", swap).groups()
    virtual_available = available + int(float(free) * {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}[unit])
    return {"available_physical_bytes": available, "available_physical_plus_free_swap_bytes": virtual_available}


def tree(root, library):
    ids = [int(x) for x in subprocess.check_output(["/bin/ps", "-axo", "pid="], text=True).split()]
    rows = [row for pid in ids if (row := native(pid, library)) is not None]
    selected = {root["pid"]}
    while True:
        added = {r["pid"] for r in rows if r["parent"] in selected and r["birth"] >= root["birth"]}
        if added <= selected:
            break
        selected |= added
    return [r for r in rows if r["pid"] in selected]


def same_identity(left, right):
    return right is not None and all(left[k] == right[k] for k in ("pid", "birth", "path"))


def operate(name, tool, args, out, manifest, library):
    qualify(manifest)
    deadline = time.monotonic() + 300
    before = memory()
    (out / f"{name}.prelaunch.json").write_text(json.dumps({"name": name, "memory": before,
        "launch_floor_bytes": 6 * GIB, "native_launched": False}, indent=2), encoding="utf-8")
    while min(before.values()) < 6 * GIB:
        if time.monotonic() >= deadline:
            raise Deferred("Deferred before launch: mapped macOS6GiB launch floors; no native compiler launched")
        time.sleep(5)
        before = memory()
    record = {"name": name, "tool": str(tool), "tool_sha256": digest(tool), "arguments": args,
              "launch_memory": before, "exit_code": None, "resource_stopped": False,
              "peak_owned_tree_resident_bytes": 0, "scope": "macOS resident-byte cap, not Windows private committed bytes"}
    stdout, stderr = out / f"{name}.stdout.log", out / f"{name}.stderr.log"
    with stdout.open("xb") as so, stderr.open("xb") as se:
        process = subprocess.Popen([str(tool), *args], cwd=ROOT, stdin=subprocess.DEVNULL,
                                   stdout=so, stderr=se, start_new_session=True)
        record["pid"] = process.pid
        identity = native(process.pid, library)
        record["native_identity"] = identity
        print(f"Actual SDK {name} PID={process.pid} native_identity={identity}", flush=True)
        try:
            while process.poll() is None:
                if identity is None:
                    identity = native(process.pid, library)
                    record["native_identity"] = identity
                if identity is None:
                    raise RuntimeError("Actual compiler native identity unavailable")
                children = tree(identity, library)
                resident = sum(p["resident"] for p in children)
                record["peak_owned_tree_resident_bytes"] = max(record["peak_owned_tree_resident_bytes"], resident)
                current = memory()
                if resident > 2 * GIB or current["available_physical_bytes"] < GIB or current["available_physical_plus_free_swap_bytes"] < 3 * GIB // 2:
                    if process.poll() is not None:
                        break
                    if not same_identity(identity, native(process.pid, library)):
                        raise RuntimeError("Refusing mismatched compiler resource stop")
                    for child in reversed(children):
                        if same_identity(child, native(child["pid"], library)):
                            os.kill(child["pid"], signal.SIGKILL)
                    record["resource_stopped"] = True
                    record["stop_memory"] = current
                    break
                time.sleep(0.25)
        except Exception as error:
            record["monitor_failed"] = True
            record["monitor_error"] = str(error)
            if process.poll() is None and identity is not None and same_identity(identity, native(process.pid, library)):
                for child in reversed(tree(identity, library)):
                    if same_identity(child, native(child["pid"], library)):
                        os.kill(child["pid"], signal.SIGKILL)
                record["monitor_stopped"] = True
            else:
                # Unknown live identity is not permission to signal another PID.
                # Leave explicit live-observer failure for CI cleanup; no fake
                # terminal or blocking unmonitored wait is allowed.
                record["monitor_stopped"] = False
            raise
        finally:
            # No elapsed native kill. An observer failure is recorded, not a pass.
            if not record.get("monitor_failed") or record.get("monitor_stopped") or process.poll() is not None:
                record["exit_code"] = process.wait()
            else:
                record["live_at_observer_failure"] = True
            (out / f"{name}.terminal.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
    qualify(manifest)
    if record["resource_stopped"] or record["exit_code"] != 0:
        raise RuntimeError(f"Actual SDK operation failed: {name}; terminal/logs preserved")
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", choices=("macosx", "iphoneos"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "shaders.json").read_text(encoding="utf-8-sig"))
    controls(manifest)
    if args.self_test:
        return
    if sys.platform != "darwin" or not args.sdk or not args.out:
        raise RuntimeError("Actual macOS host/SDK/unique output directory required")
    library = ctypes.CDLL("/usr/lib/libproc.dylib")
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(BsdInfo) != 136 or ctypes.sizeof(TaskInfo) != 96:
        raise RuntimeError("Unexpected native Darwin identity/task ABI")
    metal = Path(subprocess.check_output(["xcrun", "--sdk", args.sdk, "--find", "metal"], text=True).strip()).resolve()
    metallib = Path(subprocess.check_output(["xcrun", "--sdk", args.sdk, "--find", "metallib"], text=True).strip()).resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    operations, accepted, failed = [], [], []
    for row in qualify(manifest):
        air, binary = args.out / f"{row['Name']}.air", args.out / f"{row['Name']}.metallib"
        try:
            operations.append(operate(row["Name"] + "-compile", metal, [*FLAGS, "-c", str(ROOT / row["File"]), "-o", str(air)], args.out, manifest, library))
            operations.append(operate(row["Name"] + "-link", metallib, [str(air), "-o", str(binary)], args.out, manifest, library))
            if not binary.stat().st_size:
                raise RuntimeError("Actual linked Metal library is empty")
            accepted.append({"name": row["Name"], "source_sha256": row["Sha256"], "metallib_sha256": digest(binary), "bytes": binary.stat().st_size})
        except RuntimeError as error:
            failed.append({"name": row["Name"], "error": str(error)})
            if isinstance(error, Deferred):
                break
            terminal = args.out / f"{row['Name']}-compile.terminal.json"
            if terminal.exists() and (json.loads(terminal.read_text())["resource_stopped"] or json.loads(terminal.read_text()).get("monitor_failed")):
                break
    receipt = {"scope": "Actual SDK compilation/link only; not Metal runtime, SDL binding ABI, numerical/optical parity or performance acceptance",
               "sdk": args.sdk, "sdk_version": subprocess.check_output(["xcrun", "--sdk", args.sdk, "--show-sdk-version"], text=True).strip(),
               "manifest_sha256": digest(ROOT / "shaders.json"), "flags": FLAGS, "operations": operations,
               "accepted": accepted, "failed": failed, "success": len(accepted) == 25 and not failed}
    (args.out / "receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    print(f"Actual {args.sdk}: {len(accepted)}/25 SDK programs compiled+linked; failures={len(failed)}")
    if not receipt["success"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
