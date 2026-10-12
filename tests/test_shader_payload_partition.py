import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from shader_payload_partition import (HISTORY_ONLY, partition_scene_payloads,
                                      payload_manifest, validate_scene_partition)


def array(name, data='0,1,127,128,255'):
    return 'inline constexpr unsigned char ' + name + '[]={\n' + data + '\n};\n'


class ShaderPartitionTests(unittest.TestCase):
    def setUp(self):
        self.header = ('#pragma once\nnamespace example {\n'
                       + array('vertex_spirv')
                       + '#if defined(_WIN32)\n' + array('vertex_dxil') + '#endif\n'
                       + array('reflection_history_spirv')
                       + '#if defined(_WIN32)\n' + array('reflection_history_dxil')
                       + '#endif\n}\n')

    def test_byte_for_byte_manifest_and_idempotence(self):
        partitioned = partition_scene_payloads(self.header)
        self.assertEqual(payload_manifest(self.header), payload_manifest(partitioned))
        self.assertEqual(partitioned, partition_scene_payloads(partitioned))
        self.assertEqual(2, partitioned.count(HISTORY_ONLY))
        self.assertIn('#if defined(_WIN32)\n#if !defined(' + HISTORY_ONLY,
                      partitioned)

    def test_history_is_not_guarded(self):
        partitioned = partition_scene_payloads(self.header)
        for backend in ('spirv', 'dxil'):
            self.assertNotIn('#if !defined(' + HISTORY_ONLY + ')\n'
                             + array('reflection_history_' + backend), partitioned)

    def test_partial_guard_rejected(self):
        broken = partition_scene_payloads(self.header).replace('#endif\n', '', 1)
        with self.assertRaises(ValueError):
            validate_scene_partition(broken)

    def test_missing_history_rejected(self):
        with self.assertRaises(ValueError):
            partition_scene_payloads(array('vertex_spirv'))

    def test_duplicate_rejected(self):
        with self.assertRaises(ValueError):
            partition_scene_payloads(self.header + array('vertex_spirv'))

    def test_bad_byte_rejected(self):
        with self.assertRaises(ValueError):
            payload_manifest(array('bad_spirv', '256'))


if __name__ == '__main__':
    unittest.main()
