"""Compile the complete GLSL candidate; no native runtime/player acceptance."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root / 'tools/ci/vulkan-curved-source.json').read_text())
out = Path(os.environ['RUNNER_TEMP']) / 'vulkan-curved-r456'
out.mkdir(exist_ok=False)
receipt = {'commit': os.environ['GITHUB_SHA'], 'run_id': os.environ['GITHUB_RUN_ID'],
           'status': 'pending', 'variants': [], 'native_gpu_accepted': False,
           'player_built': False, 'production_adopted': False, 'release_published': False}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def verify_sources():
    for item in manifest['sources']:
        path = root / item['path']
        if path.stat().st_size != item['bytes'] or digest(path) != item['sha256']:
            raise RuntimeError(f"Actual source differs: {item['path']}")

try:
    verify_sources()
    receipt['source_manifest_sha256'] = digest(root / 'tools/ci/vulkan-curved-source.json')
    receipt['sources'] = len(manifest['sources'])
    receipt['tools'] = {}
    for tool in ('glslc', 'spirv-val'):
        path = Path(shutil.which(tool)).resolve(strict=True)
        receipt['tools'][tool] = {'path': str(path), 'sha256': digest(path),
                                 'version': subprocess.check_output([tool, '--version'], text=True)}
    for name in ('shadow', 'reflection', 'reflection_history', 'reflection_ground_history',
                 'reflection_liquid_history', 'reflection_liquid_motion'):
        source_name = 'reflection' if name.startswith('reflection_') and name != 'reflection_liquid_motion' else name
        source = root / f'src/render/shaders/vulkan_{source_name}_rayquery.comp'
        binary = out / f'{name}.spv'
        defines = ['-DSTARFOX_VULKAN_REFLECTION_HISTORY=1'] if name.startswith('reflection_') else []
        if name == 'reflection_ground_history':
            defines.append('-DSTARFOX_VULKAN_GROUND_HISTORY=1')
        if name == 'reflection_liquid_history':
            defines.append('-DSTARFOX_VULKAN_LIQUID_HISTORY=1')
        command = ['glslc', *defines, '-fshader-stage=compute', '--target-env=vulkan1.2',
                   '-o', str(binary), str(source)]
        subprocess.run(command, check=True)
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.2', str(binary)], check=True)
        if binary.stat().st_size % 4:
            raise RuntimeError('Unaligned SPIR-V payload')
        receipt['variants'].append({'name': name, 'bytes': binary.stat().st_size,
                                    'sha256': digest(binary), 'command': command,
                                    'validated': True})
        print(f'PASS compile/validate {name}: {binary.stat().st_size} bytes', flush=True)
    verify_sources()
    receipt['status'] = 'six_complete_variants_compiled_and_validated_not_gpu_executed'
except Exception as error:
    receipt['status'] = 'compile_or_validation_failed'
    receipt['error'] = str(error)
    raise
finally:
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
