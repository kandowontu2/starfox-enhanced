"""Partition C++ shader declarations without recompiling or changing bytecode."""
import hashlib
import re


HISTORY_ONLY = 'STARFOX_CALIBRATED_REFLECTION_HISTORY_ONLY'
_ARRAY = re.compile(
    r'inline constexpr unsigned char ([a-z_]+)\[\]=\{\n([0-9,\n]+)\n\};\n')
_GUARD = '#if !defined(' + HISTORY_ONLY + ')\n'


def payload_manifest(header):
    """Independently record the length/hash of every actual byte array."""
    result = {}
    for match in _ARRAY.finditer(header):
        name, data = match.groups()
        if name in result:
            raise ValueError('Duplicate shader payload: ' + name)
        payload = bytes(int(value) for value in re.findall(r'\d+', data))
        result[name] = {'bytes': len(payload),
                        'sha256': hashlib.sha256(payload).hexdigest()}
    if not result:
        raise ValueError('No shader byte arrays found')
    return result


def partition_scene_payloads(header):
    """Keep history's two payloads unconditional; default callers retain all."""
    if HISTORY_ONLY in header:
        # The generated form is idempotent, but reject partial/foreign guards.
        validate_scene_partition(header)
        return header

    def wrap(match):
        if match.group(1) in ('reflection_history_dxil', 'reflection_history_spirv'):
            return match.group(0)
        return _GUARD + match.group(0) + '#endif\n'

    partitioned = _ARRAY.sub(wrap, header)
    validate_scene_partition(partitioned)
    return partitioned


def validate_scene_partition(header):
    matches = list(_ARRAY.finditer(header))
    names = [match.group(1) for match in matches]
    if not matches or len(set(names)) != len(names):
        raise ValueError('Missing/duplicate scene payloads')
    if not {'reflection_history_dxil', 'reflection_history_spirv'} <= set(names):
        raise ValueError('Both history payloads must remain available')
    expected = 0
    for match in matches:
        history = match.group(1).startswith('reflection_history_')
        guarded = header[max(0, match.start() - len(_GUARD)):match.start()] == _GUARD
        if history == guarded:
            raise ValueError('Incorrect selective shader guard: ' + match.group(1))
        if guarded:
            expected += 1
            if not header[match.end():].startswith('#endif\n'):
                raise ValueError('Unclosed shader guard: ' + match.group(1))
    if header.count(HISTORY_ONLY) != expected:
        raise ValueError('Unexpected selective shader guards')
