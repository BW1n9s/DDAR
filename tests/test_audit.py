import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_installation import FormatError, canonical, parse_plugin, records, subrecords, vmad_scripts

TEMP_ROOT = Path(__file__).resolve().parents[1] / 'build' / 'test-tmp'
TEMP_ROOT.mkdir(parents=True, exist_ok=True)


def sub(tag, data):
    return tag.encode() + struct.pack('<H', len(data)) + data


def record(tag, data, fid=0, flags=0):
    return struct.pack('<4sIIIIHH', tag.encode(), len(data), flags, fid, 0, 44, 0) + data


def string(value):
    data = value.encode()
    return struct.pack('<H', len(data)) + data


class AuditTests(unittest.TestCase):
    def test_owner_uses_master_index_not_winning_plugin(self):
        self.assertEqual(canonical('Patch.esp', ['Original.esm'], 0x00000880), 'original.esm|000880')
        self.assertEqual(canonical('Patch.esp', ['Original.esm'], 0x01000880), 'patch.esp|000880')

    def test_light_file_on_disk_id_is_not_runtime_fe_id(self):
        self.assertEqual(canonical('Light.esp', ['Skyrim.esm'], 0x01000ABC), 'light.esp|000ABC')
        with self.assertRaises(FormatError):
            canonical('Light.esp', [], 0xFE001ABC)

    def test_null_reference(self):
        self.assertIsNone(canonical('a.esp', [], 0))

    def test_extended_subrecord(self):
        raw = sub('XXXX', struct.pack('<I', 8)) + b'EDID\x00\x00abcdefgh'
        self.assertEqual(list(subrecords(raw)), [('EDID', b'abcdefgh')])

    def test_truncated_subrecord_rejected(self):
        with self.assertRaises(FormatError):
            list(subrecords(b'EDID\x10\x00bad'))

    def test_vmad_object_formats(self):
        for fmt in (1, 2):
            obj = struct.pack('<IHH', 0x01000880, 0xFFFF, 0) if fmt == 1 else struct.pack('<HHI', 0, 0xFFFF, 0x01000880)
            raw = struct.pack('<HHH', 5, fmt, 1) + string('zadBeltScript') + b'\x00' + struct.pack('<H', 1)
            raw += string('deviceRendered') + b'\x01\x00' + obj
            result = vmad_scripts(raw)
            self.assertEqual(result[0]['properties']['deviceRendered'], {'form': 0x01000880})

    def test_unknown_vmad_type_fails_closed(self):
        raw = struct.pack('<HHH', 5, 2, 1) + string('Script') + b'\0\x01\x00'
        raw += string('Prop') + b'\x63\x00'
        with self.assertRaises(FormatError):
            vmad_scripts(raw)

    def test_compressed_armor_and_group_skip(self):
        data = sub('EDID', b'TestArmor\0') + sub('BOD2', struct.pack('<II', 1 << 19, 0))
        compressed = struct.pack('<I', len(data)) + zlib.compress(data)
        armor = record('ARMO', compressed, 0x00000880, 0x40000)
        group = struct.pack('<4sI4si8s', b'GRUP', 24 + len(armor), b'ARMO', 0, b'\0' * 8) + armor
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            p = Path(tmp) / 'Test.esp'
            p.write_bytes(record('TES4', b'') + group)
            header, objs = parse_plugin(p)
            self.assertEqual(objs[0]['slots'], [49])
            self.assertEqual(objs[0]['key'], 'test.esp|000880')

    def test_group_cannot_escape_file(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            p = Path(tmp) / 'Bad.esp'
            p.write_bytes(struct.pack('<4sI4si8s', b'GRUP', 100, b'ARMO', 0, b'\0' * 8))
            with self.assertRaises(FormatError):
                list(records(p))

    def test_gbk_nonlocalized_name(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            p = Path(tmp) / 'Test.esp'
            p.write_bytes(record('TES4', b'') + record('ARMO', sub('FULL', '腰带'.encode('cp936') + b'\0'), 0x880))
            self.assertEqual(parse_plugin(p, 'cp936')[1][0]['name'], '腰带')


if __name__ == '__main__':
    unittest.main()
