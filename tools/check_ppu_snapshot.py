"""Compare BG2 probe output with an isolated host layer in source RGB555.

The host's explicit magenta transparency marker is excluded; this checks
opaque sampling/colours, not transparency, sprites, or final composition.
"""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('reference', type=Path)
parser.add_argument('host_layer', type=Path)
args = parser.parse_args()
with Image.open(args.reference) as image:
    reference = image.convert('RGB')
with Image.open(args.host_layer) as image:
    host = image.convert('RGB')
if reference.size != host.size:
    parser.error(f'Image dimensions differ: {reference.size} vs {host.size}')
checked = mismatched = maximum = 0
first = None
def rgb555(colour):
    return tuple(round(channel * 31 / 255) for channel in colour)
for index, (actual, expected) in enumerate(zip(reference.get_flattened_data(), host.get_flattened_data())):
    if expected == (255, 0, 255):
        continue
    checked += 1
    maximum = max(maximum, *(abs(a - b) for a, b in zip(actual, expected)))
    if rgb555(actual) != rgb555(expected):
        mismatched += 1
        if first is None:
            first = (index % host.width, index // host.width, actual, expected)
print(f'Opaque RGB555: {checked} checked, {mismatched} mismatches; RGB888 max delta {maximum}')
if first:
    print(f'First mismatch (x, y, reference, host): {first}')
if not checked or mismatched:
    raise SystemExit(1)
