import io
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

from unity_serialized_file import (
    MAX_SERIALIZED_TYPES,
    MAX_SERIALIZED_OBJECTS,
    MAX_SERIALIZED_EXTERNALS,
    MAX_SERIALIZED_EXTERNAL_STRING_BYTES,
    MAX_SERIALIZED_USER_INFORMATION_BYTES,
    MAX_TYPE_DEPENDENCIES_PER_TYPE,
    MAX_SERIALIZED_FILE_BYTES,
    MAX_TYPE_TREE_NODES,
    MAX_TYPE_TREE_STRING_BUFFER_BYTES,
    MAX_TYPE_TREE_STRING_BYTES,
    MAX_TOTAL_TYPE_DEPENDENCIES,
    MAX_TOTAL_TYPE_TREE_NODES,
    MAX_TOTAL_TYPE_TREE_STRING_BYTES,
    UnitySerializedFileError,
    UnitySerializedExternalReference,
    UnitySerializedReferenceType,
    UnitySerializedObject,
    UnitySerializedScriptType,
    UnitySerializedType,
    UnityTypeTreeNode,
    UnitySpriteRegion,
    UnityTexture2DMetadata,
    MAX_TEXTURE2D_NAME_BYTES,
MAX_TEXTURE2D_IMAGE_BYTES,
    parse_type_tree_blob,
    resolve_type_tree_string_offset,
    resolve_type_tree_node_strings,
    parse_serialized_file_header,
    parse_serialized_file_metadata_v21,
    parse_serialized_object_table,
    parse_serialized_external_references,
    parse_serialized_reference_type_table,
    parse_serialized_script_type_table,
    parse_serialized_type_table,
    read_serialized_object_payload,
    read_sprite_texture_link_status,
    read_sprite_region,
    read_texture2d_metadata_prefix,
read_texture2d_image_data,
    read_serialized_metadata_preamble,
)
import unity_serialized_file as serialized_file_module


def serialized_file_header(metadata_size=8, file_size=64, version=21, data_offset=32, endian=0):
    return struct.pack(">IIII", metadata_size, file_size, version, data_offset) + bytes((endian, 0, 0, 0))


def serialized_file_with_metadata(metadata, endian="little"):
    file_size = 20 + len(metadata) + 4
    marker = 0 if endian == "little" else 1
    return serialized_file_header(
        metadata_size=len(metadata),
        file_size=file_size,
        data_offset=20 + len(metadata),
        endian=marker,
    ) + metadata + b"DATA"


def serialized_file_with_preamble(unity_version=b"2019.4.4f1", platform=19, endian="little", flag=1):
    byte_order = "<" if endian == "little" else ">"
    metadata = unity_version + b"\0" + struct.pack(byte_order + "i", platform) + bytes((flag,))
    return serialized_file_with_metadata(metadata, endian)


def type_tree_blob(nodes, string_buffer, endian="little"):
    byte_order = "<" if endian == "little" else ">"
    record_format = byte_order + "HBBIIiiiQ"
    records = b"".join(struct.pack(record_format, *node) for node in nodes)
    return struct.pack(byte_order + "ii", len(nodes), len(string_buffer)) + records + string_buffer


class CountingStream(io.BytesIO):
    def __init__(self, value):
        super().__init__(value)
        self.read_bytes = 0

    def read(self, size=-1):
        result = super().read(size)
        self.read_bytes += len(result)
        return result


class TestUnitySerializedFileHeader(unittest.TestCase):
    def test_v9_and_v21_headers_keep_fixed_fields_big_endian_and_read_only_20_bytes(self):
        for version in (9, 21):
            with self.subTest(version=version):
                stream = CountingStream(serialized_file_header(version=version))

                result = parse_serialized_file_header(stream, available_size=64)

                self.assertEqual(result["format_version"], version)
                self.assertEqual(result["file_size"], 64)
                self.assertEqual(result["metadata_size"], 8)
                self.assertEqual(result["metadata_offset"], 20)
                self.assertEqual(result["metadata_end"], 28)
                self.assertEqual(result["data_offset"], 32)
                self.assertEqual(result["endian"], "little")
                self.assertEqual(stream.read_bytes, 20)


class TestUnitySerializedFileMetadataPreamble(unittest.TestCase):
    def parse_preamble(self, data):
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, len(data))
        stream.read_bytes = 0
        return stream, header, read_serialized_metadata_preamble(stream, header)

    def test_reads_little_and_big_endian_preambles_within_metadata(self):
        little_stream, _, little = self.parse_preamble(serialized_file_with_preamble())
        big_stream, _, big = self.parse_preamble(
            serialized_file_with_preamble(platform=-7, endian="big", flag=0)
        )

        self.assertEqual(little, {
            "unity_version": "2019.4.4f1",
            "target_platform": 19,
            "has_type_tree": True,
        })
        self.assertEqual(big, {
            "unity_version": "2019.4.4f1",
            "target_platform": -7,
            "has_type_tree": False,
        })
        self.assertEqual(little_stream.read_bytes, 16)
        self.assertEqual(big_stream.read_bytes, 16)
        self.assertEqual(little_stream.tell(), 36)
        self.assertEqual(big_stream.tell(), 36)

    def test_accepts_maximum_version_string_and_rejects_inconsistent_header_ranges(self):
        stream, header, preamble = self.parse_preamble(
            serialized_file_with_preamble(unity_version=b"v" * 64)
        )
        self.assertEqual(preamble["unity_version"], "v" * 64)
        self.assertEqual(stream.tell(), 20 + 65 + 5)

        data = serialized_file_with_preamble()
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, len(data))
        header["metadata_end"] += 1
        stream.read_bytes = 0
        with self.assertRaises(UnitySerializedFileError):
            read_serialized_metadata_preamble(stream, header)
        self.assertEqual(stream.read_bytes, 0)


class TestUnityTypeTreeBlob(unittest.TestCase):
    def nodes(self):
        return [
            (2, 0, 1, 0, 5, -1, 0, 3, 0x1122334455667788),
            (1, 1, 0, 0x80000001, 5, 8, 2, 4, 0x8877665544332211),
        ]

    def parse(self, blob, version=21, endian="little", metadata_end=None):
        stream = CountingStream(b"\0" * 20 + blob + b"PAYLOAD")
        stream.seek(20)
        stream.read_bytes = 0
        end = 20 + len(blob) if metadata_end is None else metadata_end
        result = parse_type_tree_blob(stream, end, version, endian)
        return stream, result

    def test_v19_and_v21_node_fields_common_offsets_and_buffer_endians(self):
        strings = b"Base\0Root\0"
        for version, endian in ((19, "big"), (21, "little")):
            with self.subTest(version=version, endian=endian):
                blob = type_tree_blob(self.nodes(), strings, endian)
                stream, result = self.parse(blob, version, endian)
                self.assertEqual(result["nodes"][0], UnityTypeTreeNode(*self.nodes()[0]))
                self.assertEqual(result["nodes"][1].type_offset, 0x80000001)
                self.assertEqual(result["nodes"][1].ref_type_hash, 0x8877665544332211)
                self.assertEqual(result["string_buffer"], strings)
                self.assertEqual(stream.tell(), 20 + len(blob))
                self.assertEqual(stream.read_bytes, len(blob))

    def test_blob_cannot_cross_metadata_end_or_read_truncated_records(self):
        blob = type_tree_blob(self.nodes(), b"Base\0Root\0")
        with self.assertRaises(UnitySerializedFileError):
            self.parse(blob, metadata_end=20 + len(blob) - 1)

        partial = struct.pack("<ii", 1, 0) + b"\0" * 8
        stream = CountingStream(partial)
        with self.assertRaises(UnitySerializedFileError):
            parse_type_tree_blob(stream, 40, 21, "little")

    def test_count_and_string_buffer_limits_fail_before_node_table_read(self):
        too_many = struct.pack("<ii", MAX_TYPE_TREE_NODES + 1, 0)
        stream = CountingStream(too_many + b"\0" * 64)
        with self.assertRaises(UnitySerializedFileError):
            parse_type_tree_blob(stream, len(stream.getvalue()), 21, "little")
        self.assertEqual(stream.read_bytes, 8)

        too_large = struct.pack("<ii", 1, MAX_TYPE_TREE_STRING_BUFFER_BYTES + 1)
        stream = CountingStream(too_large + b"\0" * 64)
        with self.assertRaises(UnitySerializedFileError):
            parse_type_tree_blob(stream, len(stream.getvalue()), 21, "little")
        self.assertEqual(stream.read_bytes, 8)

    def test_invalid_local_offsets_and_level_hierarchy_fail(self):
        invalid_offset = [(1, 0, 0, 10, 0, 4, 0, 0, 0)]
        with self.assertRaises(UnitySerializedFileError):
            self.parse(type_tree_blob(invalid_offset, b"0123456789"))

        invalid_root = [(1, 1, 0, 0x80000000, 0x80000001, 4, 0, 0, 0)]
        with self.assertRaises(UnitySerializedFileError):
            self.parse(type_tree_blob(invalid_root, b""))

        invalid_jump = [
            (1, 0, 0, 0x80000000, 0x80000001, 4, 0, 0, 0),
            (1, 2, 0, 0x80000000, 0x80000001, 4, 1, 0, 0),
        ]
        with self.assertRaises(UnitySerializedFileError):
            self.parse(type_tree_blob(invalid_jump, b""))

    def test_version_and_endian_are_explicitly_bounded(self):
        blob = type_tree_blob(self.nodes(), b"Base\0Root\0")
        with self.assertRaises(UnitySerializedFileError):
            self.parse(blob, version=18)
        with self.assertRaises(UnitySerializedFileError):
            self.parse(blob, endian="invalid")

    def test_metadata_range_shorter_than_fixed_fields_fails(self):
        metadata = b"2019.4\0" + struct.pack("<i", 19)
        data = serialized_file_header(
            metadata_size=len(metadata), file_size=20 + len(metadata),
            data_offset=20 + len(metadata),
        ) + metadata
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, len(data))
        stream.read_bytes = 0

        with self.assertRaises(UnitySerializedFileError):
            read_serialized_metadata_preamble(stream, header)
        self.assertEqual(stream.read_bytes, len(b"2019.4\0"))

    def test_unterminated_long_and_non_ascii_version_strings_fail(self):
        cases = (
            serialized_file_with_metadata(b"unterminated"),
            serialized_file_with_preamble(unity_version=b"v" * 65),
            serialized_file_with_preamble(unity_version=b"v\xff"),
        )
        for data in cases:
            with self.subTest(metadata_length=len(data)):
                stream = CountingStream(data)
                header = parse_serialized_file_header(stream, len(data))
                stream.read_bytes = 0
                with self.assertRaises(UnitySerializedFileError):
                    read_serialized_metadata_preamble(stream, header)

    def test_invalid_type_tree_flag_and_header_endian_fail(self):
        data = serialized_file_with_preamble(flag=2)
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, len(data))
        stream.read_bytes = 0
        with self.assertRaises(UnitySerializedFileError):
            read_serialized_metadata_preamble(stream, header)

        data = serialized_file_with_preamble()
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, len(data))
        header["endian"] = "invalid"
        stream.read_bytes = 0
        with self.assertRaises(UnitySerializedFileError):
            read_serialized_metadata_preamble(stream, header)
        self.assertEqual(stream.read_bytes, 0)

    def test_big_endian_metadata_marker_is_reported(self):
        stream = CountingStream(serialized_file_header(version=9, endian=1))

        result = parse_serialized_file_header(stream, available_size=64)

        self.assertEqual(result["format_version"], 9)
        self.assertEqual(result["endian"], "big")
        self.assertEqual(stream.read_bytes, 20)

    def test_truncated_header_and_invalid_available_size_fail(self):
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_header(io.BytesIO(b"\0" * 19), available_size=64)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_header(io.BytesIO(b""), available_size=True)

    def test_unsupported_version_and_endian_marker_fail(self):
        for version in (8, 22):
            with self.subTest(version=version):
                stream = CountingStream(serialized_file_header(version=version))
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_file_header(stream, available_size=64)
                self.assertEqual(stream.read_bytes, 20)

        stream = CountingStream(serialized_file_header(endian=2))
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_header(stream, available_size=64)
        self.assertEqual(stream.read_bytes, 20)

    def test_declared_size_limit_and_metadata_data_ranges_are_enforced(self):
        invalid_headers = (
            serialized_file_header(file_size=65),
            serialized_file_header(file_size=MAX_SERIALIZED_FILE_BYTES + 1),
            serialized_file_header(data_offset=27),
            serialized_file_header(data_offset=65),
            serialized_file_header(metadata_size=MAX_SERIALIZED_FILE_BYTES),
        )
        for header in invalid_headers:
            with self.subTest(header=header.hex()):
                stream = CountingStream(header)
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_file_header(stream, available_size=64)
                self.assertEqual(stream.read_bytes, 20)


class TestUnitySerializedTypeTable(unittest.TestCase):
    def serialized_type(
        self,
        class_id,
        *,
        endian="little",
        stripped=0,
        script_type_index=-1,
        script_id=b"S" * 16,
        old_type_hash=b"H" * 16,
        nodes=None,
        string_buffer=b"",
        dependencies=(),
    ):
        byte_order = "<" if endian == "little" else ">"
        parts = [struct.pack(byte_order + "iBh", class_id, stripped, script_type_index)]
        if class_id == 114:
            parts.append(script_id)
        parts.extend((old_type_hash, type_tree_blob(nodes or [(1, 0, 0, 0x80000001, 0x80000002, -1, 0, 0, 0)], string_buffer, endian)))
        parts.append(struct.pack(byte_order + "i", len(dependencies)))
        if dependencies:
            parts.append(struct.pack(byte_order + "{}i".format(len(dependencies)), *dependencies))
        return b"".join(parts)

    def fixture(self, entries, *, endian="little", version=21, flag=1, count=None, suffix=b""):
        byte_order = "<" if endian == "little" else ">"
        type_count = len(entries) if count is None else count
        table = struct.pack(byte_order + "i", type_count) + b"".join(entries)
        preamble_bytes = b"2019.4.4f1\0" + struct.pack(byte_order + "i", 19) + bytes((flag,))
        metadata = preamble_bytes + table + suffix
        payload = b"OBJECT_TABLE"
        stream_bytes = serialized_file_header(
            metadata_size=len(metadata),
            file_size=20 + len(metadata) + len(payload),
            version=version,
            data_offset=20 + len(metadata),
            endian=0 if endian == "little" else 1,
        ) + metadata + payload
        stream = io.BytesIO(stream_bytes)
        header = parse_serialized_file_header(stream, len(stream_bytes))
        preamble = read_serialized_metadata_preamble(stream, header)
        return stream, header, preamble

    def test_reads_two_types_and_stops_before_object_table(self):
        root = (1, 0, 0, 0x80000001, 0x80000002, -1, 0, 0, 0)
        common = self.serialized_type(142, old_type_hash=b"A" * 16, nodes=[root], dependencies=(1,))
        script = self.serialized_type(
            114,
            script_type_index=3,
            script_id=b"M" * 16,
            old_type_hash=b"B" * 16,
            nodes=[root],
            string_buffer=b"raw",
            dependencies=(0, 1),
        )
        stream, header, preamble = self.fixture((common, script))

        result = parse_serialized_type_table(stream, header, preamble)

        first, second = result["types"]
        self.assertEqual(len(result["types"]), 2)
        self.assertIsInstance(first, UnitySerializedType)
        self.assertEqual(first.class_id, 142)
        self.assertIsNone(first.script_id)
        self.assertEqual(first.old_type_hash, b"A" * 16)
        self.assertEqual(first.type_dependencies, (1,))
        self.assertEqual(first.type_tree["nodes"], (UnityTypeTreeNode(*root),))
        self.assertEqual(first.type_tree["nodes"][0].type_offset, 0x80000001)
        self.assertEqual(second.class_id, 114)
        self.assertEqual(second.script_type_index, 3)
        self.assertEqual(second.script_id, b"M" * 16)
        self.assertEqual(second.old_type_hash, b"B" * 16)
        self.assertEqual(second.type_tree["string_buffer"], b"raw")
        self.assertEqual(second.type_dependencies, (0, 1))
        self.assertEqual(result["cursor"], header["metadata_end"])
        self.assertEqual(stream.tell(), header["metadata_end"])
        self.assertEqual(stream.read(), b"OBJECT_TABLE")

    def test_reads_big_endian_type_fields(self):
        entry = self.serialized_type(142, endian="big", script_type_index=5, dependencies=(0x1020304,))
        stream, header, preamble = self.fixture((entry,), endian="big")

        result = parse_serialized_type_table(stream, header, preamble)

        self.assertEqual(result["types"][0].class_id, 142)
        self.assertEqual(result["types"][0].script_type_index, 5)
        self.assertEqual(result["types"][0].type_dependencies, (0x1020304,))
        self.assertEqual(result["cursor"], header["metadata_end"])

    def test_requires_v21_and_enabled_type_tree(self):
        entry = self.serialized_type(142)
        for version, flag in ((19, 1), (21, 0)):
            with self.subTest(version=version, flag=flag):
                stream, header, preamble = self.fixture((entry,), version=version, flag=flag)
                cursor = stream.tell()
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_type_table(stream, header, preamble)
                self.assertEqual(stream.tell(), cursor)

    def test_rejects_invalid_counts_stripped_and_truncated_entries(self):
        for count in (-1, MAX_SERIALIZED_TYPES + 1):
            with self.subTest(count=count):
                stream, header, preamble = self.fixture((), count=count)
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_type_table(stream, header, preamble)

        invalid_stripped = self.serialized_type(142, stripped=2)
        stream, header, preamble = self.fixture((invalid_stripped,))
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_type_table(stream, header, preamble)

        truncated = self.serialized_type(142, dependencies=(7,))[:-4]
        stream, header, preamble = self.fixture((truncated,))
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_type_table(stream, header, preamble)
        self.assertEqual(stream.tell(), header["metadata_end"])
        self.assertEqual(stream.read(), b"OBJECT_TABLE")

    def test_enforces_aggregate_tree_and_dependency_limits(self):
        entry = self.serialized_type(142)
        with patch.object(serialized_file_module, "MAX_TOTAL_TYPE_TREE_NODES", 1):
            stream, header, preamble = self.fixture((entry, entry))
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_type_table(stream, header, preamble)

        entry_with_strings = self.serialized_type(142, string_buffer=b"abc")
        with patch.object(serialized_file_module, "MAX_TOTAL_TYPE_TREE_STRING_BYTES", 2):
            stream, header, preamble = self.fixture((entry_with_strings,))
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_type_table(stream, header, preamble)

        entry_with_dependency = self.serialized_type(142, dependencies=(1,))
        with patch.object(serialized_file_module, "MAX_TOTAL_TYPE_DEPENDENCIES", 1):
            stream, header, preamble = self.fixture((entry_with_dependency, entry_with_dependency))
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_type_table(stream, header, preamble)

    def test_enforces_per_type_dependency_limit(self):
        entry = self.serialized_type(142, dependencies=(1, 2))
        with patch.object(serialized_file_module, "MAX_TYPE_DEPENDENCIES_PER_TYPE", 1):
            stream, header, preamble = self.fixture((entry,))
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_type_table(stream, header, preamble)


class TestUnitySerializedObjectTable(unittest.TestCase):
    def fixture(
        self,
        rows=(),
        *,
        endian="little",
        version=21,
        flag=1,
        count=None,
        records_override=None,
        script_tail=b"SCRIPT_TABLE",
        parse_types=True,
    ):
        builder = TestUnitySerializedTypeTable("test_reads_two_types_and_stops_before_object_table")
        entries = (
            builder.serialized_type(142, endian=endian),
            builder.serialized_type(114, endian=endian, script_type_index=2),
        )
        byte_order = "<" if endian == "little" else ">"
        preamble_bytes = b"2019.4.4f1\0" + struct.pack(byte_order + "i", 19) + bytes((flag,))
        type_bytes = struct.pack(byte_order + "i", len(entries)) + b"".join(entries)
        first_record = 20 + len(preamble_bytes) + len(type_bytes) + 4
        if records_override is None:
            records = bytearray()
            cursor = first_record
            for path_id, byte_start, byte_size, type_id in rows:
                padding = (-cursor) % 4
                records.extend(b"\xA5" * padding)
                cursor += padding
                records.extend(struct.pack(byte_order + "qIIi", path_id, byte_start, byte_size, type_id))
                cursor += 20
            records = bytes(records)
        else:
            records = records_override
        object_count = len(rows) if count is None else count
        suffix = struct.pack(byte_order + "i", object_count) + records + script_tail
        stream, header, preamble = builder.fixture(
            entries, endian=endian, version=version, flag=flag, suffix=suffix
        )
        if parse_types:
            type_table = parse_serialized_type_table(stream, header, preamble)
        else:
            type_table = {"types": (), "cursor": 20 + len(preamble_bytes) + len(type_bytes)}
        return stream, header, preamble, type_table

    def test_reads_little_endian_objects_and_stops_before_script_table(self):
        stream, header, preamble, type_table = self.fixture(
            ((-123, 0, 4, 0), (456, 4, 3, 1))
        )

        result = parse_serialized_object_table(stream, header, preamble, type_table)

        first, second = result["objects"]
        self.assertIsInstance(first, UnitySerializedObject)
        self.assertEqual(first, UnitySerializedObject(-123, 0, 4, 0, 142))
        self.assertEqual(second, UnitySerializedObject(456, 4, 3, 1, 114))
        self.assertEqual(result["cursor"], stream.tell())
        self.assertEqual(stream.read(len(b"SCRIPT_TABLE")), b"SCRIPT_TABLE")
        self.assertEqual(stream.read(), b"OBJECT_TABLE")

    def test_reads_big_endian_records_and_aligns_each_path_id(self):
        stream, header, preamble, type_table = self.fixture(
            ((0x102030405, 1, 2, 1),), endian="big"
        )

        result = parse_serialized_object_table(stream, header, preamble, type_table)

        self.assertEqual(result["objects"], (UnitySerializedObject(0x102030405, 1, 2, 1, 114),))
        self.assertEqual(stream.read(len(b"SCRIPT_TABLE")), b"SCRIPT_TABLE")
        self.assertEqual(stream.read(), b"OBJECT_TABLE")

    def test_accepts_empty_table_and_rejects_excessive_count(self):
        stream, header, preamble, type_table = self.fixture(())
        result = parse_serialized_object_table(stream, header, preamble, type_table)
        self.assertEqual(result["objects"], ())
        self.assertEqual(stream.read(len(b"SCRIPT_TABLE")), b"SCRIPT_TABLE")
        self.assertEqual(stream.read(), b"OBJECT_TABLE")

        with patch.object(serialized_file_module, "MAX_SERIALIZED_OBJECTS", 1):
            for count in (-1, 2):
                with self.subTest(count=count):
                    stream, header, preamble, type_table = self.fixture((), count=count)
                    with self.assertRaises(UnitySerializedFileError):
                        parse_serialized_object_table(stream, header, preamble, type_table)

    def test_requires_v21_type_tree_and_cursor_after_type_table(self):
        for version, flag in ((19, 1), (21, 0)):
            with self.subTest(version=version, flag=flag):
                stream, header, preamble, type_table = self.fixture(
                    (), version=version, flag=flag, parse_types=False
                )
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_object_table(stream, header, preamble, type_table)

        stream, header, preamble, type_table = self.fixture(())
        stream.seek(type_table["cursor"] + 1)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_object_table(stream, header, preamble, type_table)

    def test_invalid_type_id_and_data_ranges_fail_closed(self):
        for rows in (
            ((1, 0, 1, 2),),
            ((1, 100, 1, 0),),
            ((1, 0, 1000, 0),),
        ):
            with self.subTest(rows=rows):
                stream, header, preamble, type_table = self.fixture(rows)
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_object_table(stream, header, preamble, type_table)

    def test_truncated_record_does_not_read_payload_or_return_partial_objects(self):
        stream, header, preamble, type_table = self.fixture(
            (), count=1, records_override=b"\x01", script_tail=b""
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_object_table(stream, header, preamble, type_table)
        self.assertLessEqual(stream.tell(), header["metadata_end"])
        stream.seek(header["data_offset"])
        self.assertEqual(stream.read(), b"OBJECT_TABLE")


class TestUnitySerializedScriptTypeTable(unittest.TestCase):
    def fixture(self, entries=(), *, endian="little", count=None, records_override=None, suffix=b""):
        object_fixture = TestUnitySerializedObjectTable(
            "test_reads_little_endian_objects_and_stops_before_script_table"
        )
        initial_stream, header, preamble, type_table = object_fixture.fixture(
            rows=(), endian=endian, script_tail=b""
        )
        object_table = parse_serialized_object_table(initial_stream, header, preamble, type_table)
        byte_order = "<" if endian == "little" else ">"
        if records_override is None:
            records = bytearray()
            cursor = object_table["cursor"] + 4
            for file_index, path_id in entries:
                records.extend(struct.pack(byte_order + "i", file_index))
                cursor += 4
                padding = (-cursor) % 4
                records.extend(b"\xA5" * padding)
                cursor += padding
                records.extend(struct.pack(byte_order + "q", path_id))
                cursor += 8
            records = bytes(records)
        else:
            records = records_override
        script_count = len(entries) if count is None else count
        script_bytes = struct.pack(byte_order + "i", script_count) + records
        stream, header, preamble, type_table = object_fixture.fixture(
            rows=(), endian=endian, script_tail=script_bytes + suffix
        )
        object_table = parse_serialized_object_table(stream, header, preamble, type_table)
        counted_stream = CountingStream(stream.getvalue())
        counted_stream.seek(object_table["cursor"])
        stream = counted_stream
        return stream, header, preamble, object_table

    def test_reads_little_endian_script_types_and_stops_at_next_table(self):
        stream, header, preamble, object_table = self.fixture(((0, -123), (2, 456)))
        bytes_before = stream.read_bytes

        result = parse_serialized_script_type_table(stream, header, preamble, object_table)

        self.assertEqual(result["script_types"], (UnitySerializedScriptType(0, -123), UnitySerializedScriptType(2, 456)))
        self.assertEqual(result["cursor"], stream.tell())
        self.assertEqual(stream.read_bytes - bytes_before, result["cursor"] - object_table["cursor"])
        self.assertEqual(stream.read(len(b"OBJECT_TABLE")), b"OBJECT_TABLE")

    def test_reads_big_endian_and_accepts_empty_table(self):
        stream, header, preamble, object_table = self.fixture(((1, 0x102030405),), endian="big")
        result = parse_serialized_script_type_table(stream, header, preamble, object_table)
        self.assertEqual(result["script_types"], (UnitySerializedScriptType(1, 0x102030405),))

        stream, header, preamble, object_table = self.fixture(())
        result = parse_serialized_script_type_table(stream, header, preamble, object_table)
        self.assertEqual(result["script_types"], ())

    def test_requires_v21_typetree_and_matching_object_table_cursor(self):
        stream, header, preamble, object_table = self.fixture(())
        for candidate_header, candidate_preamble, candidate_table in (
            (dict(header, format_version=20), preamble, object_table),
            (header, dict(preamble, has_type_tree=False), object_table),
            (header, preamble, dict(object_table, cursor=object_table["cursor"] + 1)),
        ):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_script_type_table(
                    stream, candidate_header, candidate_preamble, candidate_table
                )

    def test_rejects_invalid_count_file_index_and_truncated_record(self):
        stream, header, preamble, object_table = self.fixture((), count=-1)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_script_type_table(stream, header, preamble, object_table)

        stream, header, preamble, object_table = self.fixture((), count=2)
        with patch.object(serialized_file_module, "MAX_SERIALIZED_OBJECTS", 1):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_script_type_table(stream, header, preamble, object_table)

        stream, header, preamble, object_table = self.fixture(((-1, 7),))
        result = parse_serialized_script_type_table(stream, header, preamble, object_table)
        self.assertEqual(result["script_types"], (UnitySerializedScriptType(-1, 7),))

        stream, header, preamble, object_table = self.fixture(
            (), count=1, records_override=struct.pack("<i", 0) + b"\x00\x00"
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_script_type_table(stream, header, preamble, object_table)

    def test_rejects_metadata_end_before_path_id_without_reading_sentinel(self):
        stream, header, preamble, object_table = self.fixture(
            (), count=1, records_override=struct.pack("<i", 0) + b"\x00\x00"
        )
        header = dict(header, metadata_end=object_table["cursor"] + 4 + 4 + 2)
        bytes_before = stream.read_bytes
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_script_type_table(stream, header, preamble, object_table)
        expected_prefix = 4 + 4 + (-(object_table["cursor"] + 8)) % 4
        self.assertEqual(stream.read_bytes - bytes_before, expected_prefix)
        self.assertEqual(stream.tell(), object_table["cursor"] + expected_prefix)


class TestUnitySerializedExternalReferences(unittest.TestCase):
    def fixture(self, externals=(), *, endian="little", count=None, records_override=None, suffix=b""):
        byte_order = "<" if endian == "little" else ">"
        if records_override is None:
            records = bytearray()
            for asset_path, guid, file_type, path_name in externals:
                records.extend(asset_path.encode("utf-8") + b"\0")
                records.extend(guid)
                records.extend(struct.pack(byte_order + "i", file_type))
                records.extend(path_name.encode("utf-8") + b"\0")
            records = bytes(records)
        else:
            records = records_override
        external_count = len(externals) if count is None else count
        suffix = struct.pack(byte_order + "i", external_count) + records + suffix
        builder = TestUnitySerializedScriptTypeTable("test_reads_little_endian_script_types_and_stops_at_next_table")
        stream, header, preamble, object_table = builder.fixture((), endian=endian, suffix=suffix)
        script_table = parse_serialized_script_type_table(stream, header, preamble, object_table)
        return stream, header, preamble, script_table

    def test_reads_external_fields_and_stops_before_reference_types(self):
        expected = (
            UnitySerializedExternalReference("", bytes(range(16)), 1, "sharedassets0.assets"),
            UnitySerializedExternalReference("library/path", b"\x10" * 16, -2, "Assets/地图.asset"),
        )
        stream, header, preamble, script_table = self.fixture(
            tuple((item.asset_path, item.guid, item.file_type, item.path_name) for item in expected),
            endian="big",
        )
        bytes_before = stream.read_bytes

        result = parse_serialized_external_references(stream, header, preamble, script_table)

        self.assertEqual(result["externals"], expected)
        self.assertEqual(result["cursor"], stream.tell())
        self.assertEqual(stream.read_bytes - bytes_before, result["cursor"] - script_table["cursor"])
        self.assertEqual(stream.read(len(b"OBJECT_TABLE")), b"OBJECT_TABLE")

    def test_accepts_empty_external_table(self):
        stream, header, preamble, script_table = self.fixture(())
        result = parse_serialized_external_references(stream, header, preamble, script_table)
        self.assertEqual(result["externals"], ())
        self.assertEqual(stream.read(len(b"OBJECT_TABLE")), b"OBJECT_TABLE")

    def test_requires_v21_typetree_and_matching_script_cursor(self):
        stream, header, preamble, script_table = self.fixture(())
        for candidate_header, candidate_preamble, candidate_table in (
            (dict(header, format_version=20), preamble, script_table),
            (header, dict(preamble, has_type_tree=False), script_table),
            (header, preamble, dict(script_table, cursor=script_table["cursor"] + 1)),
        ):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_external_references(
                    stream, candidate_header, candidate_preamble, candidate_table
                )

    def test_rejects_bad_count_missing_nul_invalid_utf8_and_truncated_guid(self):
        stream, header, preamble, script_table = self.fixture((), count=-1)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_external_references(stream, header, preamble, script_table)

        stream, header, preamble, script_table = self.fixture((), count=2)
        with patch.object(serialized_file_module, "MAX_SERIALIZED_EXTERNALS", 1):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_external_references(stream, header, preamble, script_table)

        stream, header, preamble, script_table = self.fixture(
            (), count=1, records_override=b"unterminated"
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_external_references(stream, header, preamble, script_table)

        invalid_utf8 = b"\xFF\0" + b"\0" * 16 + struct.pack("<i", 0) + b"name\0"
        stream, header, preamble, script_table = self.fixture(
            (), count=1, records_override=invalid_utf8
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_external_references(stream, header, preamble, script_table)

        truncated_guid = b"asset\0" + b"\x00" * 2
        stream, header, preamble, script_table = self.fixture(
            (), count=1, records_override=truncated_guid
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_external_references(stream, header, preamble, script_table)

    def test_external_strings_obey_byte_limit_without_consuming_next_table(self):
        stream, header, preamble, script_table = self.fixture(
            (("abcde", b"\x00" * 16, 0, "path"),)
        )
        bytes_before = stream.read_bytes
        with patch.object(serialized_file_module, "MAX_SERIALIZED_EXTERNAL_STRING_BYTES", 4):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_external_references(stream, header, preamble, script_table)
        self.assertEqual(stream.read_bytes - bytes_before, 4 + 4)
        self.assertEqual(stream.tell(), script_table["cursor"] + 8)


class TestUnitySerializedReferenceTypeTable(unittest.TestCase):
    def reference_type(
        self,
        *,
        endian="little",
        class_id=1,
        script_type_index=-1,
        script_id=None,
        stripped=0,
        old_hash=b"\xA5" * 16,
        class_name="RefType",
        namespace="Example",
        assembly_name="Assembly-CSharp",
    ):
        byte_order = "<" if endian == "little" else ">"
        if script_id is None and (script_type_index >= 0 or class_id == 114):
            script_id = b"\x5A" * 16
        record = bytearray(struct.pack(byte_order + "i", class_id))
        record.extend(bytes((stripped,)))
        record.extend(struct.pack(byte_order + "h", script_type_index))
        if script_id is not None:
            record.extend(script_id)
        record.extend(old_hash)
        nodes = ((1, 0, 0, 0, 5, 4, 0, 0, 0),)
        record.extend(type_tree_blob(nodes, b"Root\0name\0", endian=endian))
        record.extend(class_name.encode("utf-8") + b"\0")
        record.extend(namespace.encode("utf-8") + b"\0")
        record.extend(assembly_name.encode("utf-8") + b"\0")
        return bytes(record)

    def fixture(self, records=(), *, endian="little", count=None, records_override=None, user_information=""):
        byte_order = "<" if endian == "little" else ">"
        serialized_records = b"".join(records) if records_override is None else records_override
        reference_count = len(records) if count is None else count
        suffix = struct.pack(byte_order + "i", reference_count) + serialized_records
        if user_information is not None:
            suffix += user_information.encode("utf-8") + b"\0"
        external_builder = TestUnitySerializedExternalReferences(
            "test_accepts_empty_external_table"
        )
        stream, header, preamble, script_table = external_builder.fixture((), endian=endian, suffix=suffix)
        external_table = parse_serialized_external_references(stream, header, preamble, script_table)
        return stream, header, preamble, external_table

    def test_reads_reference_types_and_conditional_script_ids(self):
        records = (
            self.reference_type(
                endian="big", class_id=114, script_type_index=1, stripped=1,
                old_hash=b"\x10" * 16, class_name="MonoBehaviour", namespace="UnityEngine"
            ),
            self.reference_type(
                endian="big", class_id=114, script_type_index=-1, script_id=b"\x22" * 16,
                class_name="LegacyBehaviour"
            ),
            self.reference_type(
                endian="big", class_id=28, script_type_index=-1, script_id=None,
                class_name="Texture2D", namespace="UnityEngine"
            ),
        )
        stream, header, preamble, external_table = self.fixture(
            records, endian="big", user_information="Sample metadata"
        )
        bytes_before = stream.read_bytes

        result = parse_serialized_reference_type_table(stream, header, preamble, external_table)

        first, second, third = result["ref_types"]
        self.assertEqual(first.class_id, 114)
        self.assertTrue(first.is_stripped)
        self.assertEqual(first.script_id, b"\x5A" * 16)
        self.assertEqual(first.old_type_hash, b"\x10" * 16)
        self.assertEqual(first.type_tree["nodes"][0].name_offset, 5)
        self.assertEqual((first.class_name, first.namespace, first.assembly_name), ("MonoBehaviour", "UnityEngine", "Assembly-CSharp"))
        self.assertEqual(second.script_id, b"\x22" * 16)
        self.assertIsNone(third.script_id)
        self.assertEqual(third.class_name, "Texture2D")
        self.assertEqual(result["user_information"], "Sample metadata")
        self.assertEqual(result["cursor"], header["metadata_end"])
        self.assertEqual(stream.read_bytes - bytes_before, result["cursor"] - external_table["cursor"])
        self.assertEqual(stream.read(len(b"OBJECT_TABLE")), b"OBJECT_TABLE")

    def test_accepts_empty_reference_table_and_empty_user_information(self):
        stream, header, preamble, external_table = self.fixture(())
        result = parse_serialized_reference_type_table(stream, header, preamble, external_table)
        self.assertEqual(result["ref_types"], ())
        self.assertEqual(result["user_information"], "")
        self.assertEqual(result["cursor"], header["metadata_end"])

    def test_requires_v21_typetree_and_matching_external_cursor(self):
        stream, header, preamble, external_table = self.fixture(())
        for candidate_header, candidate_preamble, candidate_table in (
            (dict(header, format_version=20), preamble, external_table),
            (header, dict(preamble, has_type_tree=False), external_table),
            (header, preamble, dict(external_table, cursor=external_table["cursor"] + 1)),
        ):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_reference_type_table(
                    stream, candidate_header, candidate_preamble, candidate_table
                )

    def test_rejects_counts_truncated_fields_and_aggregate_type_tree_limit(self):
        stream, header, preamble, external_table = self.fixture((), count=-1)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_reference_type_table(stream, header, preamble, external_table)

        stream, header, preamble, external_table = self.fixture((), count=2)
        with patch.object(serialized_file_module, "MAX_SERIALIZED_TYPES", 1):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_reference_type_table(stream, header, preamble, external_table)

        truncated_script_id = struct.pack("<iBh", 1, 0, 0) + b"\x00" * 4
        stream, header, preamble, external_table = self.fixture(
            (), count=1, records_override=truncated_script_id, user_information=None
        )
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_reference_type_table(stream, header, preamble, external_table)

        valid = self.reference_type()
        stream, header, preamble, external_table = self.fixture((valid,))
        with patch.object(serialized_file_module, "MAX_TOTAL_TYPE_TREE_NODES", 0):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_reference_type_table(stream, header, preamble, external_table)

    def test_rejects_unterminated_class_name_and_user_information_limit(self):
        record = self.reference_type()
        record = record[:-len(b"Assembly-CSharp\0")] + b"Assembly-CSharp"
        stream, header, preamble, external_table = self.fixture((record,), user_information=None)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_reference_type_table(stream, header, preamble, external_table)

        stream, header, preamble, external_table = self.fixture((), user_information="abcde")
        with patch.object(serialized_file_module, "MAX_SERIALIZED_USER_INFORMATION_BYTES", 4):
            with self.assertRaises(UnitySerializedFileError):
                parse_serialized_reference_type_table(stream, header, preamble, external_table)


class TestUnitySerializedObjectPayload(unittest.TestCase):
    def fixture(self, payload=b"FIRSTNEXT", declared_payload_size=None):
        data_offset = 32
        if declared_payload_size is None:
            declared_payload_size = len(payload)
        file_size = data_offset + declared_payload_size
        data = serialized_file_header(
            metadata_size=8,
            file_size=file_size,
            data_offset=data_offset,
        ) + b"\0" * 12 + payload
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, available_size=file_size)
        return stream, header

    def test_reads_selected_payload_without_consuming_neighbor(self):
        stream, header = self.fixture()
        object_record = UnitySerializedObject(1, 0, 5, 0, 142)
        bytes_before = stream.read_bytes

        result = read_serialized_object_payload(stream, header, object_record)

        self.assertEqual(result, b"FIRST")
        self.assertEqual(stream.tell(), header["data_offset"] + 5)
        self.assertEqual(stream.read_bytes - bytes_before, 5)
        self.assertEqual(stream.read(4), b"NEXT")

    def test_accepts_payload_at_data_start_and_exact_file_end(self):
        stream, header = self.fixture(payload=b"END!")
        object_record = UnitySerializedObject(2, 0, 4, 0, 142)

        self.assertEqual(read_serialized_object_payload(stream, header, object_record), b"END!")
        self.assertEqual(stream.tell(), header["file_size"])

    def test_zero_sized_payload_seeks_without_reading_bytes(self):
        stream, header = self.fixture()
        object_record = UnitySerializedObject(3, 2, 0, 0, 142)
        bytes_before = stream.read_bytes

        self.assertEqual(read_serialized_object_payload(stream, header, object_record), b"")

        self.assertEqual(stream.tell(), header["data_offset"] + 2)
        self.assertEqual(stream.read_bytes, bytes_before)

    def test_rejects_invalid_header_record_and_ranges_before_reading(self):
        _, header = self.fixture()
        invalid_cases = (
            (dict(header, format_version=20), UnitySerializedObject(1, 0, 1, 0, 142)),
            (dict(header, file_size=MAX_SERIALIZED_FILE_BYTES + 1), UnitySerializedObject(1, 0, 1, 0, 142)),
            (header, UnitySerializedObject(1, 9, 1, 0, 142)),
            (header, UnitySerializedObject(1, 0xFFFFFFFF, 1, 0, 142)),
            (header, UnitySerializedObject(1, 0, -1, 0, 142)),
        )
        for candidate_header, object_record in invalid_cases:
            with self.subTest(header=candidate_header, object_record=object_record):
                stream, _ = self.fixture()
                bytes_before = stream.read_bytes
                with self.assertRaises(UnitySerializedFileError):
                    read_serialized_object_payload(stream, candidate_header, object_record)
                self.assertEqual(stream.read_bytes, bytes_before)

    def test_rejects_truncated_stream_without_returning_partial_payload(self):
        stream, header = self.fixture(payload=b"AB", declared_payload_size=4)
        object_record = UnitySerializedObject(1, 0, 4, 0, 142)

        with self.assertRaises(UnitySerializedFileError):
            read_serialized_object_payload(stream, header, object_record)


class TestUnitySpriteTextureLink(unittest.TestCase):
    @staticmethod
    def sprite_payload(
        endian="little",
        name=b"QA_SPRITE",
        tags=(),
        file_id=0,
        path_id=7,
        rect=(0.0, 0.0, 0.0, 0.0),
        atlas_file_id=0,
        atlas_path_id=0,
    ):
        byte_order = "<" if endian == "little" else ">"
        payload = bytearray()

        def pack_i32(value):
            payload.extend(struct.pack(byte_order + "i", value))

        def pack_i64(value):
            payload.extend(struct.pack(byte_order + "q", value))

        def aligned_string(value):
            pack_i32(len(value))
            payload.extend(value)
            payload.extend(b"\0" * (-len(payload) % 4))

        aligned_string(name)
        payload.extend(struct.pack(byte_order + "4f", *rect))  # m_Rect
        payload.extend(b"\0" * 8)  # m_Offset
        payload.extend(b"\0" * 16)  # m_Border
        payload.extend(b"\0" * 4)  # m_PixelsToUnits
        payload.extend(b"\0" * 8)  # m_Pivot
        payload.extend(b"\0" * 4)  # m_Extrude
        payload.append(1)  # m_IsPolygon
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(b"\0" * 24)  # m_RenderDataKey
        pack_i32(len(tags))
        for tag in tags:
            aligned_string(tag)
        pack_i32(atlas_file_id)
        pack_i64(atlas_path_id)  # m_SpriteAtlas PPtr
        pack_i32(file_id)
        pack_i64(path_id)  # SpriteRenderData.texture PPtr
        prefix_size = len(payload)
        payload.extend(b"MESH_PIXEL_SENTINEL")
        return bytes(payload), prefix_size

    def fixture(
        self,
        endian="little",
        name=b"QA_SPRITE",
        tags=(),
        file_id=0,
        path_id=7,
        rect=(0.0, 0.0, 0.0, 0.0),
        atlas_file_id=0,
        atlas_path_id=0,
    ):
        payload, prefix_size = self.sprite_payload(
            endian=endian,
            name=name,
            tags=tags,
            file_id=file_id,
            path_id=path_id,
            rect=rect,
            atlas_file_id=atlas_file_id,
            atlas_path_id=atlas_path_id,
        )
        file_size = 32 + len(payload)
        stream = CountingStream(
            serialized_file_header(
                metadata_size=12,
                file_size=file_size,
                data_offset=32,
                endian=1 if endian == "big" else 0,
            )
            + b"\0" * 12
            + payload
        )
        header = parse_serialized_file_header(stream, available_size=file_size)
        preamble = {
            "unity_version": "2019.4.4f1",
            "target_platform": 19,
            "has_type_tree": False,
        }
        record = UnitySerializedObject(22, 0, len(payload), 0, 213)
        return stream, header, preamble, record, prefix_size

    def test_reads_bounded_integral_regions_in_both_endians(self):
        for endian in ("little", "big"):
            with self.subTest(endian=endian):
                stream, header, preamble, record, prefix_size = self.fixture(
                    endian=endian, rect=(1.0, 2.0, 3.0, 4.0)
                )
                region = read_sprite_region(
                    stream,
                    header,
                    preamble,
                    record,
                    {7: UnityTexture2DMetadata(8, 8, 0, 25)},
                )

                self.assertEqual(region, UnitySpriteRegion(1, 2, 3, 4))
                self.assertEqual(region._fields, ("x", "y", "width", "height"))
                self.assertEqual(stream.tell(), header["data_offset"] + prefix_size)
                self.assertEqual(stream.read(len(b"MESH_PIXEL_SENTINEL")), b"MESH_PIXEL_SENTINEL")

    def test_rejects_invalid_regions_atlas_and_unresolved_texture(self):
        invalid_cases = (
            {"rect": (-1.0, 0.0, 2.0, 2.0)},
            {"rect": (0.5, 0.0, 2.0, 2.0)},
            {"rect": (float("nan"), 0.0, 2.0, 2.0)},
            {"rect": (0.0, 0.0, float("inf"), 2.0)},
            {"rect": (0.0, 0.0, 0.0, 2.0)},
            {"rect": (7.0, 7.0, 2.0, 2.0)},
            {"tags": (b"atlas-one",), "rect": (0.0, 0.0, 2.0, 2.0)},
            {"atlas_path_id": 9, "rect": (0.0, 0.0, 2.0, 2.0)},
            {"file_id": 1, "rect": (0.0, 0.0, 2.0, 2.0)},
            {"path_id": 0, "rect": (0.0, 0.0, 2.0, 2.0)},
            {"path_id": 8, "rect": (0.0, 0.0, 2.0, 2.0)},
        )
        for options in invalid_cases:
            with self.subTest(options=options):
                stream, header, preamble, record, _ = self.fixture(**options)
                with self.assertRaises(UnitySerializedFileError):
                    read_sprite_region(
                        stream,
                        header,
                        preamble,
                        record,
                        {7: UnityTexture2DMetadata(8, 8, 0, 25)},
                    )

    def test_rejects_invalid_texture_metadata_and_truncated_prefix(self):
        stream, header, preamble, record, prefix_size = self.fixture(
            rect=(0.0, 0.0, 2.0, 2.0)
        )
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_region(
                stream,
                header,
                preamble,
                record,
                {7: UnityTexture2DMetadata(0, 8, 0, 25)},
            )

        with self.assertRaises(UnitySerializedFileError):
            read_sprite_region(
                stream,
                header,
                preamble,
                record._replace(byte_size=prefix_size - 1),
                {7: UnityTexture2DMetadata(8, 8, 0, 25)},
            )

        with patch.object(serialized_file_module, "MAX_SPRITE_OBJECT_PREFIX_BYTES", 8):
            with self.assertRaises(UnitySerializedFileError):
                read_sprite_region(
                    stream,
                    header,
                    preamble,
                    record,
                    {7: UnityTexture2DMetadata(8, 8, 0, 25)},
                )

    def test_rejects_unvalidated_build_and_class(self):
        stream, header, preamble, record, _ = self.fixture(rect=(0.0, 0.0, 2.0, 2.0))
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_region(
                stream,
                header,
                {"unity_version": "2020.1.0f1", "target_platform": 19},
                record,
                {7: UnityTexture2DMetadata(8, 8, 0, 25)},
            )
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_region(
                stream,
                header,
                preamble,
                record._replace(class_id=28),
                {7: UnityTexture2DMetadata(8, 8, 0, 25)},
            )

    def test_reads_aligned_strings_and_stops_at_texture_pointer(self):
        stream, header, preamble, record, prefix_size = self.fixture(
            name="MÁSCARA".encode("utf-8"),
            tags=(b"atlas-one", "tag-ç".encode("utf-8")),
        )
        bytes_before = stream.read_bytes

        result = read_sprite_texture_link_status(stream, header, preamble, record, {7})

        self.assertEqual(result, "linked")
        self.assertEqual(stream.tell(), header["data_offset"] + prefix_size)
        self.assertEqual(stream.read_bytes - bytes_before, prefix_size)
        self.assertEqual(stream.read(len(b"MESH_PIXEL_SENTINEL")), b"MESH_PIXEL_SENTINEL")

    def test_classifies_local_null_external_and_unresolved_references_in_both_endians(self):
        cases = (
            ("little", 0, 77, {77}, "linked"),
            ("big", 0, 0, {77}, "null"),
            ("little", 1, 77, {77}, "external"),
            ("big", 0, 77, set(), "unresolved"),
        )
        for endian, file_id, path_id, local_ids, expected in cases:
            with self.subTest(endian=endian, file_id=file_id, path_id=path_id):
                stream, header, preamble, record, _ = self.fixture(
                    endian=endian, file_id=file_id, path_id=path_id
                )
                self.assertEqual(
                    read_sprite_texture_link_status(stream, header, preamble, record, local_ids),
                    expected,
                )

    def test_rejects_unsupported_build_class_and_negative_file_id(self):
        invalid_contexts = (
            ({"unity_version": "2020.1.0f1", "target_platform": 19}, 213, 0),
            ({"unity_version": "2019.4.4f1", "target_platform": 0}, 213, 0),
            ({"unity_version": "2019.4.4f1", "target_platform": 19}, 28, 0),
            ({"unity_version": "2019.4.4f1", "target_platform": 19}, 213, -1),
        )
        for context, class_id, file_id in invalid_contexts:
            with self.subTest(context=context, class_id=class_id, file_id=file_id):
                stream, header, preamble, record, _ = self.fixture(file_id=file_id)
                with self.assertRaises(UnitySerializedFileError):
                    read_sprite_texture_link_status(
                        stream,
                        header,
                        context,
                        record._replace(class_id=class_id),
                        {7},
                    )

    def test_rejects_truncated_payload_strings_and_aggregate_limits(self):
        stream, header, preamble, record, prefix_size = self.fixture()
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_texture_link_status(
                stream, header, preamble, record._replace(byte_size=prefix_size - 1), {7}
            )

        stream, header, preamble, record, _ = self.fixture(name=b"long-name")
        with patch.object(serialized_file_module, "MAX_SPRITE_NAME_BYTES", 2):
            with self.assertRaises(UnitySerializedFileError):
                read_sprite_texture_link_status(stream, header, preamble, record, {7})

        stream, header, preamble, record, _ = self.fixture(tags=(b"first", b"second"))
        with patch.object(serialized_file_module, "MAX_SPRITE_ATLAS_TAGS", 1):
            with self.assertRaises(UnitySerializedFileError):
                read_sprite_texture_link_status(stream, header, preamble, record, {7})

        stream, header, preamble, record, _ = self.fixture(tags=(b"too-long",))
        with patch.object(serialized_file_module, "MAX_SPRITE_ATLAS_TAG_BYTES", 2):
            with self.assertRaises(UnitySerializedFileError):
                read_sprite_texture_link_status(stream, header, preamble, record, {7})

    def test_rejects_prefix_limit_and_physically_truncated_stream(self):
        stream, header, preamble, record, _ = self.fixture()
        with patch.object(serialized_file_module, "MAX_SPRITE_OBJECT_PREFIX_BYTES", 8):
            with self.assertRaises(UnitySerializedFileError):
                read_sprite_texture_link_status(stream, header, preamble, record, {7})

        stream, header, preamble, record, _ = self.fixture()
        truncated_stream = CountingStream(stream.getvalue()[:-len(b"MESH_PIXEL_SENTINEL") - 2])
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_texture_link_status(
                truncated_stream, header, preamble, record, {7}
            )

    def test_rejects_invalid_local_texture_index(self):
        stream, header, preamble, record, _ = self.fixture()
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_texture_link_status(stream, header, preamble, record, [7])
        with self.assertRaises(UnitySerializedFileError):
            read_sprite_texture_link_status(stream, header, preamble, record, {True})


if __name__ == "__main__":
    unittest.main()
class TestUnityTexture2DMetadataPrefix(unittest.TestCase):
    def fixture(self, endian="little", name=b"secret", pixels=b"PIXEL_BYTES"):
        byte_order = "<" if endian == "little" else ">"
        payload = bytearray(struct.pack(byte_order + "i", len(name)))
        payload.extend(name)
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(struct.pack(byte_order + "iB", 0, 1))
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(struct.pack(byte_order + "iiii", 64, 32, 4096, 4))
        fields_end = len(payload)
        payload.extend(pixels)

        file_size = 32 + len(payload)
        data = (
            serialized_file_header(
                metadata_size=12,
                file_size=file_size,
                data_offset=32,
                endian=1 if endian == "big" else 0,
            )
            + b"\0" * 12
            + bytes(payload)
        )
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, available_size=len(data))
        preamble = {
            "unity_version": "2019.4.4f1",
            "target_platform": 19,
            "has_type_tree": False,
        }
        record = UnitySerializedObject(7, 0, len(payload), 0, 28)
        return stream, header, preamble, record, fields_end

    def test_reads_little_and_big_endian_fields_without_name_or_pixels(self):
        for endian in ("little", "big"):
            with self.subTest(endian=endian):
                stream, header, preamble, record, fields_end = self.fixture(endian=endian)
                bytes_before = stream.read_bytes

                result = read_texture2d_metadata_prefix(stream, header, preamble, record)

                self.assertEqual(result.width, 64)
                self.assertEqual(result.height, 32)
                self.assertEqual(result.complete_image_size, 4096)
                self.assertEqual(result.texture_format, 4)
                self.assertEqual(stream.tell(), header["data_offset"] + fields_end)
                self.assertEqual(stream.read_bytes - bytes_before, 21)

    def test_accepts_type_tree_flag_for_validated_texture_layout(self):
        stream, header, preamble, record, _ = self.fixture()
        preamble["has_type_tree"] = True

        result = read_texture2d_metadata_prefix(stream, header, preamble, record)

        self.assertEqual((result.width, result.height), (64, 32))

    def test_rejects_unsupported_context_and_class(self):
        stream, header, preamble, record, _ = self.fixture()
        invalid_contexts = (
            ({**preamble, "unity_version": "2019.4.5f1"}, record),
            ({**preamble, "target_platform": 0}, record),
            ({**preamble, "has_type_tree": "true"}, record),
            (preamble, record._replace(class_id=1)),
        )
        for invalid_preamble, invalid_record in invalid_contexts:
            with self.subTest(preamble=invalid_preamble, record=invalid_record):
                with self.assertRaises(UnitySerializedFileError):
                    read_texture2d_metadata_prefix(stream, header, invalid_preamble, invalid_record)

    def test_rejects_overlong_name_invalid_bool_and_short_object(self):
        stream, header, preamble, record, _ = self.fixture(name=b"x" * (MAX_TEXTURE2D_NAME_BYTES + 1))
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, record)

        stream, header, preamble, record, _ = self.fixture()
        name_end = header["data_offset"] + 4 + len(b"secret")
        fallback_flag_offset = ((name_end + 3) & ~3) + 4
        stream.seek(fallback_flag_offset)
        stream.write(b"\x02")
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, record)

        short_record = record._replace(byte_size=35)
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, short_record)

    def test_rejects_truncated_prefix_and_invalid_values(self):
        stream, header, preamble, record, _ = self.fixture()
        stream.truncate(header["data_offset"] + 35)
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, record)

        stream, header, preamble, record, _ = self.fixture()
        negative_dimensions = struct.pack("<iiii", -1, 32, 4096, 4)
        name_end = header["data_offset"] + 4 + len(b"secret")
        fields_offset = ((name_end + 3) & ~3) + 8
        stream.seek(fields_offset)
        stream.write(negative_dimensions)
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, record)


    def test_rejects_negative_name_length(self):
        stream, header, preamble, record, _ = self.fixture()
        stream.seek(header["data_offset"])
        stream.write(struct.pack("<i", -1))

        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_metadata_prefix(stream, header, preamble, record)


class TestUnitySerializedFileMetadataV21(unittest.TestCase):
    def fixture(self, endian="little", include_records=True, suffix=b"", version=21, type_tree_flag=1):
        byte_order = "<" if endian == "little" else ">"
        metadata = bytearray(b"2019.4.4f1\0")
        metadata.extend(struct.pack(byte_order + "i", 19))
        metadata.append(type_tree_flag)

        if include_records:
            string_buffer = b"Root\0Node\0"
            node = struct.pack(byte_order + "HBBIIiiiQ", 1, 0, 0, 0, 5, 0, 0, 0, 0)
            type_tree = struct.pack(byte_order + "ii", 1, len(string_buffer)) + node + string_buffer
            metadata.extend(struct.pack(byte_order + "i", 1))
            metadata.extend(struct.pack(byte_order + "iBh", 1, 0, -1))
            metadata.extend(b"\xA5" * 16)
            if type_tree_flag:
                metadata.extend(type_tree)
                metadata.extend(struct.pack(byte_order + "i", 0))
            metadata.extend(struct.pack(byte_order + "i", 1))
            padding = (-(20 + len(metadata))) % 4
            metadata.extend(b"\0" * padding)
            metadata.extend(struct.pack(byte_order + "qIIi", 7, 0, 0, 0))
        else:
            metadata.extend(struct.pack(byte_order + "i", 0))
            metadata.extend(struct.pack(byte_order + "i", 0))

        metadata.extend(struct.pack(byte_order + "i", 0))
        metadata.extend(struct.pack(byte_order + "i", 0))
        metadata.extend(struct.pack(byte_order + "i", 0))
        metadata.extend(b"ok\0")
        metadata.extend(suffix)

        metadata_end = 20 + len(metadata)
        payload = b"OBJECT_PAYLOAD_MUST_NOT_BE_READ"
        file_size = metadata_end + len(payload)
        endian_marker = 0 if endian == "little" else 1
        header = struct.pack(">IIII", len(metadata), file_size, version, metadata_end)
        header += bytes((endian_marker, 0, 0, 0))
        return CountingStream(header + metadata + payload), metadata_end

    def test_composes_all_tables_in_both_endians_without_reading_payload(self):
        for endian in ("little", "big"):
            with self.subTest(endian=endian):
                stream, metadata_end = self.fixture(endian=endian)
                result = parse_serialized_file_metadata_v21(stream, len(stream.getvalue()))

                self.assertEqual(result["header"]["format_version"], 21)
                self.assertEqual(result["preamble"]["unity_version"], "2019.4.4f1")
                self.assertEqual(len(result["types"]), 1)
                self.assertEqual(result["types"][0].class_id, 1)
                self.assertEqual(len(result["objects"]), 1)
                self.assertEqual(result["objects"][0].path_id, 7)
                self.assertEqual(result["script_types"], ())
                self.assertEqual(result["externals"], ())
                self.assertEqual(result["ref_types"], ())
                self.assertEqual(result["user_information"], "ok")
                self.assertEqual(result["cursor"], metadata_end)
                self.assertEqual(stream.tell(), metadata_end)
                self.assertEqual(stream.read_bytes, metadata_end)

    def test_empty_tables_are_composed(self):
        stream, metadata_end = self.fixture(include_records=False)
        result = parse_serialized_file_metadata_v21(stream, len(stream.getvalue()))

        self.assertEqual(result["types"], ())
        self.assertEqual(result["objects"], ())
        self.assertEqual(result["cursor"], metadata_end)

    def test_rejects_non_v21_or_type_tree_disabled(self):
        for version, flag in ((20, 1), (21, 0)):
            with self.subTest(version=version, flag=flag):
                stream, _ = self.fixture(version=version, type_tree_flag=flag)
                with self.assertRaises(UnitySerializedFileError):
                    parse_serialized_file_metadata_v21(stream, len(stream.getvalue()))

    def test_type_tree_disabled_mode_reads_metadata_without_payload(self):
        stream, metadata_end = self.fixture(type_tree_flag=0)
        result = parse_serialized_file_metadata_v21(
            stream, len(stream.getvalue()), allow_type_tree_disabled=True
        )

        self.assertFalse(result["preamble"]["has_type_tree"])
        self.assertEqual(len(result["types"]), 1)
        self.assertEqual(result["types"][0].type_tree, {"nodes": (), "string_buffer": b""})
        self.assertEqual(len(result["objects"]), 1)
        self.assertEqual(result["objects"][0].class_id, 1)
        self.assertEqual(result["cursor"], metadata_end)
        self.assertEqual(stream.tell(), metadata_end)
        self.assertEqual(stream.read_bytes, metadata_end)

    def test_type_tree_disabled_mode_requires_explicit_boolean(self):
        stream, _ = self.fixture(type_tree_flag=0)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(stream, len(stream.getvalue()), allow_type_tree_disabled=1)

    def test_type_tree_disabled_mode_rejects_invalid_flags_versions_counts_and_truncation(self):
        invalid_flag, _ = self.fixture(type_tree_flag=2)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(
                invalid_flag, len(invalid_flag.getvalue()), allow_type_tree_disabled=True
            )

        unsupported_version, _ = self.fixture(version=20, type_tree_flag=0)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(
                unsupported_version, len(unsupported_version.getvalue()), allow_type_tree_disabled=True
            )

        oversized_count, _ = self.fixture(type_tree_flag=0)
        oversized_bytes = bytearray(oversized_count.getvalue())
        struct.pack_into("<i", oversized_bytes, 36, MAX_SERIALIZED_TYPES + 1)
        oversized_count = CountingStream(oversized_bytes)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(
                oversized_count, len(oversized_bytes), allow_type_tree_disabled=True
            )

        truncated, _ = self.fixture(type_tree_flag=0)
        truncated_bytes = truncated.getvalue()[:-1]
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(
                CountingStream(truncated_bytes), len(truncated_bytes), allow_type_tree_disabled=True
            )

    def test_explicit_disabled_mode_preserves_enabled_metadata_result(self):
        default_stream, _ = self.fixture()
        explicit_stream, _ = self.fixture()
        default_result = parse_serialized_file_metadata_v21(
            default_stream, len(default_stream.getvalue())
        )
        explicit_result = parse_serialized_file_metadata_v21(
            explicit_stream, len(explicit_stream.getvalue()), allow_type_tree_disabled=True
        )
        self.assertEqual(explicit_result, default_result)

    def test_rejects_trailing_metadata_and_truncated_final_string(self):
        trailing_stream, _ = self.fixture(suffix=b"\x00")
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(trailing_stream, len(trailing_stream.getvalue()))

        truncated_stream, _ = self.fixture()
        data = truncated_stream.getvalue()
        metadata_size = struct.unpack_from(">I", data, 0)[0]
        shortened_header = struct.pack(">I", metadata_size - 1) + data[4:]
        truncated_stream = CountingStream(shortened_header)
        with self.assertRaises(UnitySerializedFileError):
            parse_serialized_file_metadata_v21(truncated_stream, len(shortened_header))
class TestUnityTypeTreeStringResolver(unittest.TestCase):
    def node(self, type_offset, name_offset):
        return UnityTypeTreeNode(1, 0, 0, type_offset, name_offset, 0, 0, 0, 0)

    def test_resolves_local_and_common_strings_separately(self):
        local = b"int\0position\0"
        common = b"Vector3\0m_Script\0"
        node = self.node(0x80000000, 0x80000008)
        local_before = bytes(local)
        resolved = resolve_type_tree_node_strings(node, local, common)

        self.assertEqual(resolved, {"type_name": "Vector3", "name": "m_Script"})
        self.assertEqual(resolve_type_tree_string_offset(0, local), "int")
        self.assertEqual(resolve_type_tree_string_offset(4, local), "position")
        self.assertEqual(node.type_offset, 0x80000000)
        self.assertEqual(local, local_before)

    def test_accepts_empty_strings_and_string_ending_at_buffer_limit(self):
        self.assertEqual(resolve_type_tree_string_offset(0, b"\0"), "")
        self.assertEqual(resolve_type_tree_string_offset(0, b"x\0"), "x")

    def test_rejects_invalid_offsets_buffers_terminators_and_utf8(self):
        for offset in (-1, 1 << 32, True, 2):
            with self.subTest(offset=offset):
                with self.assertRaises(UnitySerializedFileError):
                    resolve_type_tree_string_offset(offset, b"x\0")

        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0x80000000, b"", None)
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0x80000002, b"", b"x\0")
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0, b"unterminated")
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0, b"\xFF\0")
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0, b"x" * (MAX_TYPE_TREE_STRING_BYTES + 1) + b"\0")

    def test_rejects_invalid_node_and_oversized_string_buffers(self):
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_node_strings(object(), b"x\0")
        with self.assertRaises(UnitySerializedFileError):
            resolve_type_tree_string_offset(0, b"\0" * (MAX_TYPE_TREE_STRING_BUFFER_BYTES + 1))


class TestUnityTypeTreeObjectValueReader(unittest.TestCase):
    def make_tree(self, endian="little", include_unknown=False, common=False):
        strings = bytearray()
        offsets = {}

        def offset(value):
            if value not in offsets:
                offsets[value] = len(strings)
                strings.extend(value.encode("utf-8") + b"\0")
            return offsets[value] | (0x80000000 if common else 0)

        specifications = [
            ("Texture2D", "Base", 0, -1, 0),
            ("SInt32", "m_Width", 1, 4, 0),
            ("bool", "m_Enabled", 1, 1, 1 << 14),
            ("string", "m_Label", 1, -1, 0),
            ("Array", "Array", 2, -1, 0),
            ("int", "size", 3, 4, 0),
            ("char", "data", 3, 1, 0),
            ("Array", "m_Samples", 1, -1, 0),
            ("int", "size", 2, 4, 0),
            ("SInt32", "data", 2, 4, 0),
            ("TypelessData", "m_ImageData", 1, -1, 0),
            ("int", "size", 2, 4, 0),
            ("UInt8", "data", 2, 1, 0),
            ("SInt32", "m_Height", 1, 4, 0),
        ]
        if include_unknown:
            specifications.append(("UnsupportedPayload", "m_Unknown", 1, 1, 0))
        nodes = tuple(
            UnityTypeTreeNode(
                1, level, 0, offset(type_name), offset(name), byte_size, 0, meta_flags, 0
            )
            for type_name, name, level, byte_size, meta_flags in specifications
        )
        tree = {"nodes": nodes, "string_buffer": b"" if common else bytes(strings)}
        return tree, bytes(strings) if common else None

    def make_payload(self, endian="little", label=b"Demo", samples=(10, 20, 30), image=b"PXLS"):
        order = "<" if endian == "little" else ">"
        payload = bytearray(struct.pack(order + "i", 128))
        payload.append(1)
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(struct.pack(order + "i", len(label)))
        payload.extend(label)
        payload.extend(struct.pack(order + "i", len(samples)))
        if samples:
            payload.extend(struct.pack(order + "i" * len(samples), *samples))
        payload.extend(struct.pack(order + "i", len(image)))
        payload.extend(image)
        payload.extend(struct.pack(order + "i", 64))
        return bytes(payload)

    def decode(self, payload, tree, endian="little", fields=("m_Width", "m_Height"), common=None):
        return serialized_file_module.decode_type_tree_object_fields(
            payload, tree, endian, fields, common
        )

    def test_decodes_selected_values_strings_arrays_and_alignment_in_both_endians(self):
        for endian in ("little", "big"):
            tree, _ = self.make_tree(endian)
            result = self.decode(
                self.make_payload(endian),
                tree,
                endian,
                ("m_Width", "m_Height", "m_Enabled", "m_Label"),
            )
            self.assertEqual(
                result,
                {"m_Width": 128, "m_Enabled": True, "m_Label": "Demo", "m_Height": 64},
            )

    def test_common_strings_are_resolved_without_mutating_the_tree(self):
        tree, common = self.make_tree(common=True)
        before = tree["nodes"]
        result = self.decode(self.make_payload(), tree, fields=("m_Width",), common=common)
        self.assertEqual(result, {"m_Width": 128})
        self.assertEqual(tree["nodes"], before)

    def test_truncation_invalid_utf8_and_unsupported_leaf_fail_without_partial_result(self):
        tree, _ = self.make_tree()
        with self.assertRaises(UnitySerializedFileError):
            self.decode(self.make_payload()[:-1], tree)
        with self.assertRaises(UnitySerializedFileError):
            self.decode(self.make_payload(label=b"\xFF"), tree)

        unsupported, _ = self.make_tree(include_unknown=True)
        with self.assertRaises(UnitySerializedFileError):
            self.decode(self.make_payload() + b"\0", unsupported)

    def test_array_count_and_payload_bounds_are_enforced(self):
        tree, _ = self.make_tree()
        payload = bytearray(self.make_payload())
        # The bounded sample array count follows the width, aligned bool and string.
        sample_count_offset = 16
        payload[sample_count_offset : sample_count_offset + 4] = struct.pack("<i", 100_001)
        with self.assertRaises(UnitySerializedFileError):
            self.decode(bytes(payload), tree)
        with self.assertRaises(UnitySerializedFileError):
            self.decode(self.make_payload(image=b""), tree, fields=("m_Width", "missing"))
        large_image = self.decode(
            self.make_payload(image=b"P" * 100_001), tree, fields=("m_Width",)
        )
        self.assertEqual(large_image, {"m_Width": 128})

        truncated_image = bytearray(self.make_payload(image=b""))
        truncated_image[32:36] = struct.pack("<i", 1)
        with self.assertRaises(UnitySerializedFileError):
            self.decode(bytes(truncated_image), tree)


class TestUnityTexture2DImageData(unittest.TestCase):
    def fixture(
        self,
        endian="little",
        texture_format=1,
        width=2,
        height=2,
        image_data=b"\x01\x02\x03\x04",
        complete_image_size=None,
        mip_count=1,
        stream_offset=0,
        stream_size=0,
        stream_path=b"",
        trailer=b"",
    ):
        byte_order = "<" if endian == "little" else ">"
        if complete_image_size is None:
            complete_image_size = len(image_data) if image_data else stream_size
        payload = bytearray(struct.pack(byte_order + "i", 7) + b"texture")
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(struct.pack(byte_order + "iB", 0, 1))
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(
            struct.pack(
                byte_order + "iiii",
                width,
                height,
                complete_image_size,
                texture_format,
            )
        )
        payload.extend(struct.pack(byte_order + "i", mip_count))
        payload.extend(b"\0" * 48)
        payload.extend(struct.pack(byte_order + "i", len(image_data)))
        payload.extend(image_data)
        payload.extend(b"\0" * (-len(payload) % 4))
        payload.extend(struct.pack(byte_order + "IIi", stream_offset, stream_size, len(stream_path)))
        payload.extend(stream_path)
        payload.extend(b"\0" * (-len(payload) % 4))

        file_size = 32 + len(payload) + len(trailer)
        data = (
            serialized_file_header(
                metadata_size=12,
                file_size=file_size,
                version=21,
                data_offset=32,
                endian=1 if endian == "big" else 0,
            )
            + b"\0" * 12
            + bytes(payload)
            + trailer
        )
        stream = CountingStream(data)
        header = parse_serialized_file_header(stream, available_size=len(data))
        preamble = {
            "unity_version": "2019.4.4f1",
            "target_platform": 19,
            "has_type_tree": False,
        }
        record = UnitySerializedObject(7, 0, len(payload), 0, 28)
        return stream, header, preamble, record

    def decode(self, **kwargs):
        stream, header, preamble, record = self.fixture(**kwargs)
        result = read_texture2d_image_data(stream, header, preamble, record)
        return stream, header, result

    def test_reads_supported_inline_formats_in_both_endians(self):
        cases = (
            (1, 2, 2, bytes(range(4))),
            (3, 2, 2, bytes(range(12))),
            (4, 2, 2, bytes(range(16))),
            (25, 4, 4, bytes(range(16))),
        )
        for endian in ("little", "big"):
            for texture_format, width, height, image_data in cases:
                with self.subTest(endian=endian, texture_format=texture_format):
                    stream, header, preamble, record = self.fixture(
                        endian=endian,
                        texture_format=texture_format,
                        width=width,
                        height=height,
                        image_data=image_data,
                    )
                    bytes_before = stream.read_bytes
                    result = read_texture2d_image_data(stream, header, preamble, record)
                    self.assertEqual(result.storage, "inline")
                    self.assertEqual(result.image_data, image_data)
                    self.assertEqual(result.complete_image_size, len(image_data))
                    self.assertEqual(stream.read_bytes - bytes_before, 62 + len(image_data))
                    self.assertEqual(stream.tell(), header["file_size"])

    def test_reads_mip_chain_size_and_streams_external_data_without_path(self):
        stream, header, result = self.decode(
            texture_format=4,
            width=2,
            height=2,
            mip_count=2,
            image_data=bytes(range(20)),
        )
        self.assertEqual(result.storage, "inline")
        self.assertEqual(result.mip_count, 2)
        self.assertEqual(len(result.image_data), 20)

        external_path = b"..\\private\\stream.resS"
        stream, header, preamble, record = self.fixture(
            texture_format=25,
            width=4,
            height=4,
            image_data=b"",
            complete_image_size=16,
            stream_offset=64,
            stream_size=16,
            stream_path=external_path,
        )
        bytes_before = stream.read_bytes
        external = read_texture2d_image_data(stream, header, preamble, record)
        self.assertEqual(external.storage, "external")
        self.assertIsNone(external.image_data)
        self.assertFalse(hasattr(external, "path"))
        self.assertNotIn(external_path.decode("ascii"), repr(external))
        self.assertEqual(stream.read_bytes - bytes_before, 62)

    def test_empty_texture_is_distinguished_from_external_storage(self):
        _, _, result = self.decode(
            texture_format=4,
            width=0,
            height=0,
            image_data=b"",
            complete_image_size=0,
        )
        self.assertEqual(result.storage, "empty")
        self.assertEqual(result.image_data, b"")

    def test_rejects_bad_sizes_formats_mips_and_stream_paths(self):
        invalid = (
            {"texture_format": 4, "width": 2, "height": 2, "image_data": b"short"},
            {"texture_format": 99, "width": 2, "height": 2, "image_data": b"\0" * 16},
            {"texture_format": 4, "width": 2, "height": 2, "mip_count": 3, "image_data": b"\0" * 20},
            {
                "texture_format": 25,
                "width": 4,
                "height": 4,
                "image_data": b"",
                "complete_image_size": 16,
                "stream_size": 8,
                "stream_path": b"external.resS",
            },
            {
                "texture_format": 25,
                "width": 4,
                "height": 4,
                "image_data": b"",
                "complete_image_size": 16,
                "stream_size": 16,
                "stream_path": b"",
            },
            {
                "texture_format": 25,
                "width": 4,
                "height": 4,
                "image_data": b"",
                "complete_image_size": 16,
                "stream_offset": 0xFFFFFFF0,
                "stream_size": 16,
                "stream_path": b"external.resS",
            },
            {
                "texture_format": 25,
                "width": 4,
                "height": 4,
                "image_data": bytes(range(16)),
                "complete_image_size": 16,
                "stream_size": 16,
                "stream_path": b"external.resS",
            },
        )
        for options in invalid:
            with self.subTest(options=options):
                stream, header, preamble, record = self.fixture(**options)
                with self.assertRaises(UnitySerializedFileError):
                    read_texture2d_image_data(stream, header, preamble, record)

    def test_enforces_image_limit_and_object_boundary(self):
        with patch.object(serialized_file_module, "MAX_TEXTURE2D_IMAGE_BYTES", 8):
            stream, header, preamble, record = self.fixture(
                texture_format=4,
                width=2,
                height=2,
                image_data=b"\0" * 16,
            )
            with self.assertRaises(UnitySerializedFileError):
                read_texture2d_image_data(stream, header, preamble, record)

        stream, header, preamble, record = self.fixture(
            texture_format=4,
            width=2,
            height=2,
            image_data=b"\0" * 16,
            trailer=b"NEXT_OBJECT_SENTINEL",
        )
        short_record = record._replace(byte_size=record.byte_size - 2)
        with self.assertRaises(UnitySerializedFileError):
            read_texture2d_image_data(stream, header, preamble, short_record)
        self.assertLessEqual(stream.tell(), header["data_offset"] + short_record.byte_size)
