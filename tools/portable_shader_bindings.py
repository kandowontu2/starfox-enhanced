"""Validate SDL's explicitly assigned Metal buffer slots."""
import re


def validate_metal_bindings(text, expected=None):
    resources = re.findall(r'\b(\w+)\s*\[\[buffer\((\d+)\)\]\]', text)
    names = [name for name, _ in resources]
    slots = [int(slot) for _, slot in resources]
    if not resources or len(names) != len(set(names)) or len(slots) != len(set(slots)):
        raise RuntimeError('Missing or duplicate Metal buffer binding')
    if expected is not None and dict((name, int(slot)) for name, slot in resources) != expected:
        raise RuntimeError('Unmapped or incorrectly mapped Metal buffer binding')
