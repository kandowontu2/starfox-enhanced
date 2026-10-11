"""Offline complete25 Apple build adapter; no GitHub/artifact/runtime calls.

The held compiler observer remains the sole SDK compiler launcher. Failed
attempts remain intact and cannot publish a build output. Compilation and
linkage do not qualify GPU execution, history reuse, or game performance.
"""
import argparse
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def load_source(root):
    root = root.resolve(strict=True)
    spec = importlib.util.spec_from_file_location('offline_held_embedding', root / 'tools/check_embedding.py')
    embedding = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(embedding)
    sdk = embedding.sdk
    if sdk.digest(root / 'shaders.json') != embedding.MANIFEST_SHA:
        raise RuntimeError('Complete source manifest changed')
    if sdk.digest(root / 'tools/embedding-inputs.json') != embedding.INPUTS_SHA:
        raise RuntimeError('Complete factory recipes changed')
    manifest = json.loads((root / 'shaders.json').read_text())
    # Keep configure-time inventory machine-readable without hiding controls.
    with contextlib.redirect_stdout(sys.stderr):
        sdk.controls(manifest)
    rows = sdk.qualify(manifest)
    held = json.loads((root / 'tools/embedding-inputs.json').read_text())
    if set(held['Recipes']) != sdk.NAMES:
        raise RuntimeError('Incomplete original recipe inventory')
    return embedding, rows, held


def verify_library(embedding, row, sdk_name, directory, context=None):
    sdk = embedding.sdk
    receipt = json.loads((directory / 'receipt.json').read_text())
    expected = (sdk_name, sdk.SDK_TARGETS[sdk_name], sdk.FLAGS[sdk_name],
                embedding.MANIFEST_SHA, row['Name'], 25, 1)
    actual = tuple(receipt[k] for k in ('sdk', 'target', 'flags', 'manifest_sha256',
                                      'selected_shader', 'full_source_inventory', 'expected_compiled_programs'))
    if actual != expected or receipt['success'] is not True or receipt['failed']:
        raise RuntimeError('Wrong/incomplete/failed native SDK receipt')
    if len(receipt['accepted']) != 1 or len(receipt['operations']) != 2:
        raise RuntimeError('Exact compile and library-link terminals required')
    if context is not None:
        if (context['sdk'], context['root'], context['version']) != (
                receipt['sdk'], receipt['sdk_root'], receipt['sdk_version']):
            raise RuntimeError('Cached library does not match the configured SDK')
        for frontend, operation in zip(('metal', 'metallib'), receipt['operations']):
            if operation['tool_sha256'] != context[frontend + '_sha256']:
                raise RuntimeError('Cached library does not match the configured compiler')
    accepted = receipt['accepted'][0]
    binary = directory / (row['Name'] + '.metallib')
    if (accepted['name'], accepted['entry'], accepted['source_sha256'].lower(),
        accepted['metallib_sha256'].lower(), accepted['bytes']) != (
            row['Name'], row['Entry'], row['Sha256'].lower(), sdk.digest(binary), binary.stat().st_size):
        raise RuntimeError('Native payload/source/entry bytes differ')
    sdk.qualify_metallib(binary.read_bytes(), row['Entry'])
    for suffix, operation in zip(('compile', 'link'), receipt['operations']):
        name = row['Name'] + '-' + suffix
        terminal = json.loads((directory / (name + '.terminal.json')).read_text())
        if terminal != operation or operation['name'] != name or operation['exit_code'] != 0:
            raise RuntimeError('Native operation is not its successful actual terminal')
        if any(operation.get(key) for key in ('resource_stopped', 'monitor_failed',
                                             'resource_limit_exceeded_at_retirement', 'live_at_observer_failure')):
            raise RuntimeError('Resource/observer failure is not a usable compiler output')
        if not operation.get('native_identity') or not operation.get('last_verified_native_identity'):
            raise RuntimeError('Actual native compiler identity missing')
        if operation['sdk_environment'] != {'SDKROOT': receipt['sdk_root']}:
            raise RuntimeError('Compiler SDK environment differs')
        for kind in ('stdout', 'stderr'):
            if not (directory / (name + '.' + kind + '.log')).is_file():
                raise RuntimeError('Native compiler log missing')
    compile_args = receipt['operations'][0]['arguments']
    prefix = [*sdk.FLAGS[sdk_name], '-target', sdk.SDK_TARGETS[sdk_name], '-isysroot', receipt['sdk_root'], '-c']
    if compile_args[:len(prefix)] != prefix or len(compile_args) != len(prefix) + 3:
        raise RuntimeError('Strict native compiler flags/target differ')
    source, out_switch, air = compile_args[len(prefix):]
    if not source.endswith('/' + row['File']) or out_switch != '-o' or not air.endswith('/' + row['Name'] + '.air'):
        raise RuntimeError('Wrong compiler source/air arguments')
    link_args = receipt['operations'][1]['arguments']
    if len(link_args) != 3 or link_args[:2] != [air, '-o'] or not link_args[2].endswith('/' + binary.name):
        raise RuntimeError('Wrong native library-link inputs')
    return receipt


def bundle(embedding, rows, held, sdk_name, libraries, out, context=None):
    expected = {f'metal-{sdk_name}-{row["Name"]}' for row in rows}
    actual = {p.name for p in libraries.iterdir() if p.is_dir()}
    if expected != actual:
        raise RuntimeError('Exactly all25 SDK program directories required')
    receipts = [verify_library(embedding, row, sdk_name, libraries / f'metal-{sdk_name}-{row["Name"]}', context) for row in rows]
    if len({(r['sdk_version'], r['sdk_root']) for r in receipts}) != 1:
        raise RuntimeError('A mixed-SDK bundle is not a single native build')
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bundle-', dir=out) as staging:
        stage = Path(staging)
        _, expected_bytes = embedding.generate(sdk_name, rows, held['Recipes'], libraries, stage)
        for name in ('libraries.S', 'embedding_programs.hpp'):
            os.replace(stage / name, out / name)
    return {'scope': 'Complete25 offline build inputs only; no GPU/runtime/game acceptance',
            'sdk': sdk_name, 'count': 25, 'embedded_bytes': len(expected_bytes),
            'manifest_sha256': embedding.MANIFEST_SHA,
            'libraries': {r['accepted'][0]['name']: r['accepted'][0]['metallib_sha256'] for r in receipts}}


def compile_one(embedding, rows, sdk_name, name, libraries, context):
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple SDK host required; no native substitute')
    row = next(r for r in rows if r['Name'] == name)
    libraries.parent.mkdir(parents=True, exist_ok=True)
    # Preserve every native attempt separately, including refusals/failures.
    attempt = Path(tempfile.mkdtemp(prefix=name + '-', dir=libraries.parent)) / 'native'
    result = subprocess.run([sys.executable, str(embedding.ROOT / 'tools/compile_metal.py'),
                             '--sdk', sdk_name, '--shader', name, '--out', str(attempt)], check=False)
    if result.returncode:
        raise RuntimeError(f'Native compiler refused/failed; attempt preserved: {attempt}')
    verify_library(embedding, row, sdk_name, attempt, context)
    stable = libraries / f'metal-{sdk_name}-{name}'
    stable.mkdir(parents=True, exist_ok=True)
    files = [p for p in attempt.iterdir() if p.is_file() and p.suffix != '.air']
    # Receipt last: failed or interrupted copies cannot create a usable bundle.
    files.sort(key=lambda p: p.name == 'receipt.json')
    for source in files:
        pending = stable / (source.name + '.pending')
        shutil.copyfile(source, pending)
        os.replace(pending, stable / source.name)
    verify_library(embedding, row, sdk_name, stable, context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--libraries', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--shader')
    parser.add_argument('--context', type=Path)
    parser.add_argument('mode', choices=('inventory', 'compile', 'bundle'))
    args = parser.parse_args()
    embedding, rows, held = load_source(args.source)
    context = json.loads(args.context.read_text()) if args.context else None
    if args.mode == 'inventory':
        print(';'.join(row['Name'] for row in rows))
    elif args.mode == 'compile':
        if not args.libraries or args.shader not in embedding.sdk.NAMES or context is None:
            raise RuntimeError('Selected exact shader, configured SDK and output directory required')
        compile_one(embedding, rows, args.sdk, args.shader, args.libraries, context)
    else:
        if not args.libraries or not args.out:
            raise RuntimeError('Complete library and generated bundle directories required')
        print(json.dumps(bundle(embedding, rows, held, args.sdk, args.libraries, args.out, context)))


if __name__ == '__main__':
    main()
