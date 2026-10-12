"""Fail closed if the straight-line native scene SHBIN has incomplete/repeated outputs.

Inspect compiled instruction destination masks, not source text. Conservatively
require all four lanes of every enabled output register to be written once.
This also covers unused lanes and avoids relying on an emulator's vec4 defaults.
This is a gate for pica_scene.v.pica, not a general control-flow shader verifier.
"""
import argparse
import json
from pathlib import Path
import struct


def validate_shader(data):
    def word(offset):
        if offset < 0 or offset + 4 > len(data):
            raise ValueError("Truncated SHBIN word")
        return struct.unpack_from("<I", data, offset)[0]

    def table(offset, count, stride):
        if count < 1 or offset < 0 or offset + count * stride > len(data):
            raise ValueError("Invalid SHBIN table bounds")

    if data[:4] != b"DVLB" or word(4) != 1:
        raise ValueError("Expected one native vertex shader")
    dvlp, dvle = 12, word(8)
    if data[dvlp:dvlp + 4] != b"DVLP" or data[dvle:dvle + 4] != b"DVLE":
        raise ValueError("Invalid shader headers")
    if (word(dvle + 4) >> 16) & 0xff:
        raise ValueError("Geometry shader is not supported by this gate")
    code, instructions = dvlp + word(dvlp + 8), word(dvlp + 12)
    descriptors, descriptor_count = dvlp + word(dvlp + 16), word(dvlp + 20)
    outputs, output_count = dvle + word(dvle + 40), word(dvle + 44)
    table(code, instructions, 4)
    table(descriptors, descriptor_count, 8)
    table(outputs, output_count, 8)
    begin, end = word(dvle + 8), word(dvle + 12)
    if not (0 <= begin < end <= instructions <= 512) or descriptor_count > 128:
        raise ValueError("Invalid native shader instruction bounds")
    enabled = set()
    for index in range(output_count):
        kind, register, mask = struct.unpack_from("<HHB", data, outputs + index * 8)
        if kind not in (0, 2, 3, 4, 5) or register >= 7 or not (0 < mask < 16):
            raise ValueError("Unexpected native shader output declaration")
        enabled.add(register)
    if enabled != set(range(len(enabled))):
        raise ValueError("Native output registers must be consecutive")
    writes = {register: [0] * 4 for register in enabled}
    finished = False
    for index in range(begin, end):
        instruction = word(code + index * 4)
        opcode = instruction >> 26
        if opcode == 0x22:  # END
            if index != end - 1:
                raise ValueError("Instructions after native shader END")
            finished = True
            continue
        # Only these straight-line arithmetic instructions occur in the scene
        # shader. Reject CALL/IF/LOOP, unknown instructions or an absent END.
        if opcode not in (0, 2, 8, 0x13) and not 0x30 <= opcode <= 0x3f:
            raise ValueError("Unsupported native shader instruction/control flow")
        mad = opcode >= 0x30
        register = (instruction >> (24 if mad else 21)) & 31
        descriptor = instruction & (31 if mad else 127)
        if descriptor >= descriptor_count:
            raise ValueError("Invalid operand descriptor")
        mask = word(descriptors + descriptor * 8) & 15
        if register < 16:
            if register not in writes:
                raise ValueError("Write to unconfigured native output")
            for lane in range(4):
                if mask & (8 >> lane):
                    writes[register][lane] += 1
    if not finished or any(count != 1 for lanes in writes.values() for count in lanes):
        raise ValueError("Native shader must write every enabled output lane exactly once")
    return {"instructions": end - begin, "output_registers": sorted(enabled),
            "output_lane_writes": writes, "complete_single_writes": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shader", type=Path)
    args = parser.parse_args()
    print(json.dumps(validate_shader(args.shader.read_bytes()), indent=2))


if __name__ == "__main__":
    main()
