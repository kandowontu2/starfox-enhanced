"""Exercise the offline adapter with actual retained complete50 SDK artifacts.

This does not launch an Apple compiler/driver or accept a native build graph.
Fixture mutations are never written back into the retained producer evidence.
"""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', required=True, type=Path)
parser.add_argument('--evidence', required=True, type=Path)
parser.add_argument('--out', required=True, type=Path)
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('adapter', Path(__file__).with_name('build_metal_bundle.py'))
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)
embedding, rows, held = adapter.load_source(args.source)
checks, summaries = 0, []
args.out.mkdir(parents=True, exist_ok=False)

for sdk_name in ('macosx', 'iphoneos'):
    evidence = args.evidence / sdk_name
    summary = adapter.bundle(embedding, rows, held, sdk_name, evidence, args.out / sdk_name)
    summaries.append(summary)
    checks += 25
    original_out = args.out / ('original-' + sdk_name)
    original_out.mkdir()
    embedding.generate(sdk_name, rows, held['Recipes'], evidence, original_out)
    for name in ('libraries.S', 'embedding_programs.hpp'):
        if (original_out / name).read_bytes() != (args.out / sdk_name / name).read_bytes():
            raise RuntimeError('Offline adapter changed original complete25 embedding')
        checks += 1
    # All25 kernels: mutate each accepted source hash, and its own native
    # terminal/receipt together to ensure consistency alone cannot bless failure.
    with tempfile.TemporaryDirectory(prefix='refusals-', dir=args.out) as temporary:
        fixture = Path(temporary) / 'kernel'
        for row in rows:
            fixture.mkdir()
            source = evidence / f'metal-{sdk_name}-{row["Name"]}'
            for item in source.iterdir():
                if item.is_file():
                    shutil.copyfile(item, fixture / item.name)
            original = json.loads((fixture / 'receipt.json').read_text())
            context = {'sdk': sdk_name, 'root': original['sdk_root'], 'version': original['sdk_version'],
                       'metal_sha256': original['operations'][0]['tool_sha256'],
                       'metallib_sha256': original['operations'][1]['tool_sha256']}
            adapter.verify_library(embedding, row, sdk_name, fixture, context)
            checks += 1
            for key in context:
                changed_context = dict(context)
                changed_context[key] = 'different-' + str(context[key])
                try:
                    adapter.verify_library(embedding, row, sdk_name, fixture, changed_context)
                except RuntimeError:
                    checks += 1
                else:
                    raise RuntimeError('Wrong SDK/compiler context was accepted')
            mutations = []
            changed = copy.deepcopy(original)
            changed['accepted'][0]['source_sha256'] = '0' * 64
            mutations.append((changed, None))
            for key, value in (('resource_stopped', True), ('monitor_failed', True),
                               ('exit_code', 1), ('live_at_observer_failure', True)):
                changed = copy.deepcopy(original)
                changed['operations'][0][key] = value
                mutations.append((changed, changed['operations'][0]))
            for changed, terminal in mutations:
                (fixture / 'receipt.json').write_text(json.dumps(changed))
                (fixture / (row['Name'] + '-compile.terminal.json')).write_text(
                    json.dumps(terminal or original['operations'][0]))
                try:
                    adapter.verify_library(embedding, row, sdk_name, fixture)
                except RuntimeError:
                    checks += 1
                else:
                    raise RuntimeError('Mutated native/source failure was accepted')
            shutil.rmtree(fixture)
    # An actual incomplete directory inventory refuses before publishing files.
    try:
        adapter.bundle(embedding, rows[:-1], held, sdk_name, evidence, args.out / ('incomplete-' + sdk_name))
    except RuntimeError:
        if (args.out / ('incomplete-' + sdk_name)).exists():
            raise RuntimeError('Refused incomplete bundle wrote generated output')
        checks += 1
    else:
        raise RuntimeError('Incomplete inventory was accepted')

receipt = {'scope': 'Offline adapter actual retained50 SDK byte/receipt tests and mutation refusals only; no new native/GPU build',
           'checks': checks, 'bundles': summaries,
           'adapter_sha256': embedding.sdk.digest(Path(adapter.__file__)),
           'cmake_sha256': embedding.sdk.digest(Path(__file__).resolve().parents[1] / 'cmake/BuildMetalReflection.cmake')}
(args.out / 'checks.json').write_text(json.dumps(receipt, indent=2))
print(f'PASS {checks} offline adapter checks; complete50 original payloads/metadata unchanged; no native/GPU launch')
