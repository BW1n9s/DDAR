import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_native import addresses

TEMP_ROOT = Path(__file__).resolve().parents[1] / 'build' / 'test-tmp'
TEMP_ROOT.mkdir(parents=True, exist_ok=True)


def library(entries, count=1, fmt=1, pointer_size=8):
    name = b'SkyrimSE.exe'
    return struct.pack('<6i', fmt, 1, 5, 97, 0, len(name)) + name + struct.pack('<2i', pointer_size, count) + entries


class AddressInspectionTests(unittest.TestCase):
    def parse(self, data, ids):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            path = Path(tmp) / 'synthetic.bin'
            path.write_bytes(data)
            return addresses(path, set(ids))

    def test_full_mapping_and_version(self):
        result = self.parse(library(b'\0' + struct.pack('<2Q', 17361, 0x227070)), [17361])
        self.assertEqual(result['version'], (1, 5, 97, 0))
        self.assertEqual(result['offsets'], {17361: 0x227070})

    def test_scaled_relative_offset(self):
        data = library(b'\0' + struct.pack('<2Q', 100, 0x1000) + b'\x91', count=2)
        self.assertEqual(self.parse(data, [101])['offsets'], {101: 0x1008})

    def test_independent_byte_deltas(self):
        data = library(b'\0' + struct.pack('<2Q', 100, 0x1000) + b'\x22\x04\x06', count=2)
        self.assertEqual(self.parse(data, [104])['offsets'], {104: 0x1006})

    def test_unsupported_type_rejected(self):
        with self.assertRaises(ValueError):
            self.parse(library(b'\x0f'), [1])

    def test_wrong_format_and_pointer_width_rejected(self):
        for data in [library(b'', fmt=2), library(b'', pointer_size=4)]:
            with self.assertRaises(ValueError): self.parse(data, [1])

    def test_truncation_rejected(self):
        with self.assertRaises(struct.error): self.parse(library(b'\0\x01'), [1])


if __name__ == '__main__': unittest.main()
