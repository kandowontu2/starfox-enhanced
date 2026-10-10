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
SDK_TARGETS = {'macosx': 'air64-apple-macosx13.0', 'iphoneos': 'air64-apple-ios16.0'}


class Deferred(RuntimeError):
    """A prelaunch resource refusal, not a failed shader compiler."""


def digest(path):
    held = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            held.update(block)
    return held.hexdigest()


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
    allowed = {'/actual/compiler': 'held', '/verified/versioned/compiler': 'held2'}
    versioned = {**owner, 'path': '/verified/versioned/compiler'}
    if not authorized_exec(owner, versioned, allowed):
        raise RuntimeError('Pinned same-incarnation exec control rejected')
    for changed in ({**versioned, 'pid': 43}, {**versioned, 'birth': [1, 3]},
                    {**versioned, 'path': '/unverified/compiler'}):
        if authorized_exec(owner, changed, allowed):
            raise RuntimeError('Reused PID/birth or unverified exec path accepted')
    for empty in (b'', b'MTLB' + bytes(88), b'MTLB' + bytes(1024)):
        try:
            qualify_metallib(empty, 'actual_kernel')
        except RuntimeError:
            continue
        raise RuntimeError('Empty/missing-entry library accepted')
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


def authorized_exec(left, right, allowed):
    return (right is not None and all(left[k] == right[k] for k in ('pid', 'birth'))
            and right['path'] in allowed)


def pin_sdk_executables(frontend):
    # Xcode16.4's real launcher execs usr/metal/32023/bin/metal without
    # changing PID/birth. Freeze existing matching binaries BEFORE launch;
    # never authorize an arbitrary path merely because it shares a PID.
    toolchain = frontend.parents[2]
    if not toolchain.name.endswith('.xctoolchain') or frontend.parent != toolchain / 'usr/bin':
        raise RuntimeError('Unexpected Xcode SDK toolchain frontend layout')
    paths = [frontend]
    version_root = toolchain / 'usr/metal'
    if version_root.exists():
        for version in sorted(version_root.iterdir()):
            candidate = version / 'bin' / frontend.name
            if version.name.isdecimal() and candidate.is_file():
                paths.append(candidate)
    allowed = {}
    for candidate in paths:
        real = candidate.resolve(strict=True)
        if not real.is_relative_to(toolchain.resolve()) or real.name not in (frontend.name, 'air-lld'):
            raise RuntimeError('SDK executable escaped held toolchain or changed tool')
        allowed[str(real)] = digest(real)
    return allowed


def qualify_metallib(data, entry):
    # The old resolved air-lld invocation warned that it ignored the input AIR
    # and produced a 92-byte empty MTLB. Reject that actual failure and a missing
    # exact kernel NAME token. This is not a GPU library-load/PSO proof.
    if len(data) <= 92 or data[:4] != b'MTLB' or entry.encode('utf-8') + b'\0' not in data:
        raise RuntimeError('Empty/invalid Metal library or exact kernel entry missing')


def observed_identity(process, expected, library, record, allowed):
    # A libproc query can lose the task while waitpid has not reported its
    # retirement yet. Re-poll the same owned child, never restart or signal an
    # unknown PID. Record both actual identities if a real mismatch remains.
    for attempt in range(3):
        actual = native(process.pid, library)
        if process.poll() is not None:
            return None
        if actual is not None:
            if actual['path'] not in allowed or (expected is not None and not authorized_exec(expected, actual, allowed)):
                record['identity_mismatch'] = {'expected': expected, 'actual': actual}
                raise RuntimeError('Actual compiler incarnation or unverified executable changed')
            if expected is None or actual['path'] != expected['path']:
                if digest(Path(actual['path'])) != allowed[actual['path']]:
                    record['identity_mismatch'] = {'expected': expected, 'actual': actual}
                    raise RuntimeError('Prelaunch-pinned SDK executable bytes changed')
                if expected is not None:
                    record.setdefault('verified_exec_transitions', []).append({'before': expected, 'after': actual,
                        'executable_sha256': allowed[actual['path']]})
            return actual
        time.sleep(0.05)
    record['identity_mismatch'] = {'expected': expected, 'actual': None}
    raise RuntimeError('Owned live compiler identity unavailable after bounded re-poll')


def operate(name, tool, args, out, manifest, library, sdk_environment):
    qualify(manifest)
    allowed = pin_sdk_executables(tool)
    deadline = time.monotonic() + 300
    before = memory()
    (out / f"{name}.prelaunch.json").write_text(json.dumps({"name": name, "memory": before,
        "launch_floor_bytes": 6 * GIB, "native_launched": False}, indent=2), encoding="utf-8")
    while min(before.values()) < 6 * GIB:
        if time.monotonic() >= deadline:
            raise Deferred("Deferred before launch: mapped macOS6GiB launch floors; no native compiler launched")
        time.sleep(5)
        before = memory()
    # Cloud-only recipe on14GiB runners. Preserve6GiB prelaunch and original
    # global running floors, leave3GiB launch headroom outside the owned tree,
    # and cap that tree at6GiB. This does not pass the earlier2GiB recipe.
    resident_cap = min(6 * GIB, before['available_physical_bytes'] - 3 * GIB)
    record = {"name": name, "tool": str(tool), "tool_realpath": os.path.realpath(tool),
              "tool_sha256": digest(tool), "arguments": args, "sdk_environment": sdk_environment,
              'prelaunch_pinned_executables': allowed,
              "launch_memory": before, "exit_code": None, "resource_stopped": False,
              "peak_owned_tree_resident_bytes": 0, 'owned_tree_resident_cap_bytes': resident_cap,
              "scope": "Cloud14GiB host-adapted resident cap, not the earlier2GiB recipe or Windows private committed bytes"}
    stdout, stderr = out / f"{name}.stdout.log", out / f"{name}.stderr.log"
    with stdout.open("xb") as so, stderr.open("xb") as se:
        process = subprocess.Popen([str(tool), *args], cwd=ROOT, stdin=subprocess.DEVNULL,
                                   stdout=so, stderr=se, start_new_session=True, env={**os.environ, **sdk_environment})
        record["pid"] = process.pid
        identity = native(process.pid, library)
        record["native_identity"] = identity
        print(f"Actual SDK {name} PID={process.pid} native_identity={identity}", flush=True)
        try:
            while process.poll() is None:
                actual = observed_identity(process, identity, library, record, allowed)
                if actual is None:
                    break
                if identity is None:
                    record['native_identity'] = actual
                identity = actual
                record['last_verified_native_identity'] = actual
                children = tree(identity, library)
                resident = sum(p["resident"] for p in children)
                record["peak_owned_tree_resident_bytes"] = max(record["peak_owned_tree_resident_bytes"], resident)
                current = memory()
                if resident > resident_cap or current["available_physical_bytes"] < GIB or current["available_physical_plus_free_swap_bytes"] < 3 * GIB // 2:
                    if process.poll() is not None:
                        record['resource_limit_exceeded_at_retirement'] = True
                        break
                    actual = observed_identity(process, identity, library, record, allowed)
                    if actual is None:
                        record['resource_limit_exceeded_at_retirement'] = True
                        break
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
    if any(digest(Path(path)) != held for path, held in allowed.items()):
        raise RuntimeError('Held SDK executable bytes changed during operation')
    if record["resource_stopped"] or record.get('resource_limit_exceeded_at_retirement') or record["exit_code"] != 0:
        raise RuntimeError(f"Actual SDK operation failed: {name}; terminal/logs preserved")
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", choices=("macosx", "iphoneos"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument('--shader', choices=sorted(NAMES))
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
    # Preserve tool argv[0]: metallib is a multicall/symlink frontend, not a
    # request to invoke the resolved air-lld with its default target/version.
    metal = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'metal'], text=True).strip()).absolute()
    metallib = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'metallib'], text=True).strip()).absolute()
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    sdk_environment = {'SDKROOT': sdk_root}
    target_flags = ['-target', SDK_TARGETS[args.sdk], '-isysroot', sdk_root]
    args.out.mkdir(parents=True, exist_ok=False)
    operations, accepted, failed = [], [], []
    selected = [row for row in qualify(manifest) if args.shader is None or row['Name'] == args.shader]
    for row in selected:
        air, binary = args.out / f"{row['Name']}.air", args.out / f"{row['Name']}.metallib"
        try:
            operations.append(operate(row["Name"] + "-compile", metal, [*FLAGS, *target_flags, "-c", str(ROOT / row["File"]), "-o", str(air)], args.out, manifest, library, sdk_environment))
            operations.append(operate(row["Name"] + "-link", metallib, [str(air), "-o", str(binary)], args.out, manifest, library, sdk_environment))
            qualify_metallib(binary.read_bytes(), row['Entry'])
            link_log = (args.out / f"{row['Name']}-link.stderr.log").read_text()
            if re.search(r'ignoring (?:file|input)|warning:', link_log, re.I):
                raise RuntimeError('Linker warned or ignored an input; not a valid SDK library pass')
            accepted.append({"name": row["Name"], 'entry': row['Entry'], "source_sha256": row["Sha256"],
                             'air_sha256': digest(air), "metallib_sha256": digest(binary), "bytes": binary.stat().st_size,
                             'exact_entry_token_present': True})
        except RuntimeError as error:
            failed.append({"name": row["Name"], "error": str(error)})
            if isinstance(error, Deferred):
                break
            terminal = args.out / f"{row['Name']}-compile.terminal.json"
            if terminal.exists() and (json.loads(terminal.read_text())["resource_stopped"] or json.loads(terminal.read_text()).get("monitor_failed")):
                break
    receipt = {"scope": "Actual SDK compilation/link only; not Metal runtime, SDL binding ABI, numerical/optical parity or performance acceptance",
               "sdk": args.sdk, "sdk_version": subprocess.check_output(["xcrun", "--sdk", args.sdk, "--show-sdk-version"], text=True).strip(),
               "manifest_sha256": digest(ROOT / "shaders.json"), "flags": FLAGS,
               'target': SDK_TARGETS[args.sdk], 'sdk_root': sdk_root, 'selected_shader': args.shader,
               'full_source_inventory':25, 'expected_compiled_programs':len(selected), "operations": operations,
               "accepted": accepted, "failed": failed, "success": len(accepted) == len(selected) and not failed}
    (args.out / "receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    print(f"Actual {args.sdk}: {len(accepted)}/{len(selected)} selected SDK programs compiled+linked; complete25 source pins checked; failures={len(failed)}")
    if not receipt["success"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
