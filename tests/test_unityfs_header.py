import io
import stat
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import unityfs_header


def unityfs_bundle(
    signature=b"UnityFS",
    version=6,
    player=b"5.x.x",
    engine=b"2019.4.4f1",
    flags=0x42,
    compressed_info=b"INFO",
    compressed_info_size=None,
    uncompressed_size=32,
):
    fields = (
        signature + b"\0"
        + struct.pack(">I", version)
        + player + b"\0"
        + engine + b"\0"
    )
    trailer_size = 8 + 12
    total_size = len(fields) + trailer_size + len(compressed_info) + 3
    header = fields + struct.pack(
        ">QIII", total_size,
        len(compressed_info) if compressed_info_size is None else compressed_info_size,
        uncompressed_size, flags
    )
    return header + compressed_info + b"PAY"


def unityfs_data_bundle(engine, archive_flags, blocks, nodes, payload_size=None):
    block_info = bytearray(16)
    block_info.extend(struct.pack(">i", len(blocks)))
    for uncompressed_size, compressed_size, flags in blocks:
        block_info.extend(struct.pack(">IIH", uncompressed_size, compressed_size, flags))
    block_info.extend(struct.pack(">i", len(nodes)))
    for offset, size, flags, path in nodes:
        block_info.extend(struct.pack(">qqI", offset, size, flags))
        block_info.extend(path.encode("utf-8") + b"\0")

    fields = b"UnityFS\0" + struct.pack(">I", 6) + b"5.x.x\0" + engine.encode("ascii") + b"\0"
    header_size = len(fields) + 20
    info_at_end = bool(archive_flags & unityfs_header.BLOCK_INFO_AT_END)
    data_start = header_size if info_at_end else header_size + len(block_info)
    padding_size = (-data_start) % 16 if archive_flags & unityfs_header.BLOCK_INFO_NEED_PADDING_AT_START else 0
    data_bytes = sum(block[1] for block in blocks) if payload_size is None else payload_size
    payload = b"P" * data_bytes
    if info_at_end:
        body = b"\0" * padding_size + payload + bytes(block_info)
    else:
        body = bytes(block_info) + b"\0" * padding_size + payload
    total_size = header_size + len(body)
    header = fields + struct.pack(">QIII", total_size, len(block_info), len(block_info), archive_flags)
    return header + body


class CountingStream(io.BytesIO):
    def __init__(self, value):
        super().__init__(value)
        self.bytes_read = 0

    def read(self, size=-1):
        data = super().read(size)
        self.bytes_read += len(data)
        return data


class UnityFSHeaderTests(unittest.TestCase):
    def test_valid_v6_header_reads_only_header_fields(self):
        bundle = unityfs_bundle()
        stream = CountingStream(bundle)
        result = unityfs_header.parse_header(stream, len(bundle))
        self.assertEqual(result["signature"], "UnityFS")
        self.assertEqual(result["format_version"], 6)
        self.assertEqual(result["player_version"], "5.x.x")
        self.assertEqual(result["engine_version"], "2019.4.4f1")
        self.assertEqual(result["compression"], "lz4")
        self.assertEqual(result["total_size"], len(bundle))
        self.assertEqual(stream.bytes_read, result["header_size"])
        self.assertLess(stream.bytes_read, len(bundle))

    def test_rejects_signature_and_unsupported_version(self):
        for bundle, message in (
            (unityfs_bundle(signature=b"UnityWeb"), "signature"),
            (unityfs_bundle(version=7), "format version"),
        ):
            with self.subTest(message=message):
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

    def test_rejects_truncated_and_unterminated_strings(self):
        valid = unityfs_bundle()
        for bundle in (valid[:8], valid[:20], b"UnityFS\0\x00\x00\x00\x06unterminated"):
            with self.subTest(length=len(bundle)):
                with self.assertRaises(unityfs_header.UnityFSHeaderError):
                    unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

    def test_rejects_inconsistent_total_size_and_block_info_sizes(self):
        valid = bytearray(unityfs_bundle())
        valid[valid.index(b"INFO") - 20:valid.index(b"INFO") - 12] = struct.pack(
            ">Q", len(valid) + 1
        )
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "total size"):
            unityfs_header.parse_header(io.BytesIO(valid), len(valid))

        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "remaining bundle"):
            bundle = unityfs_bundle(compressed_info_size=8)
            unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "nonzero"):
            bundle = unityfs_bundle(uncompressed_size=0)
            unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "reader limit"):
            bundle = unityfs_bundle(uncompressed_size=unityfs_header.MAX_UNCOMPRESSED_INFO_BYTES + 1)
            unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

    def test_rejects_reserved_and_unknown_compression_flags(self):
        for flags, message in ((0x400, "unsupported bits"), (0x44, "compression type")):
            with self.subTest(flags=flags):
                bundle = unityfs_bundle(flags=flags)
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.parse_header(io.BytesIO(bundle), len(bundle))

    def test_reads_regular_file_path_and_rejects_symlink(self):
        with tempfile.TemporaryDirectory(dir=str(REPOSITORY_ROOT)) as directory:
            root = Path(directory)
            bundle = root / "scene.bundle"
            bundle.write_bytes(unityfs_bundle())
            self.assertEqual(unityfs_header.read_path(bundle)["format_version"], 6)
            link = root / "scene-link.bundle"
            try:
                link.symlink_to(bundle)
            except (OSError, NotImplementedError) as error:
                self.skipTest("file symlink creation unavailable: " + str(error))
            with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "link"):
                unityfs_header.read_path(link)

    def test_rejects_simulated_reparse_parent(self):
        with tempfile.TemporaryDirectory(dir=str(REPOSITORY_ROOT)) as directory:
            bundle = Path(directory) / "scene.bundle"
            bundle.write_bytes(unityfs_bundle())
            original_lstat = Path.lstat

            def lstat_with_reparse(path):
                if path == Path(directory):
                    return SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=0x400)
                return original_lstat(path)

            with mock.patch.object(Path, "lstat", autospec=True, side_effect=lstat_with_reparse):
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "reparse"):
                    unityfs_header.read_path(bundle)


class UnityFSDataBlockLayoutTests(unittest.TestCase):
    def test_maps_blocks_when_block_info_is_at_the_start(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", 0,
            [(3, 3, 0), (2, 2, 0)],
            [(0, 5, 0, "node.bin")],
        )
        result = unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))
        expected_start = result["header_size"] + result["block_info_compressed_size"]
        self.assertEqual(result["data_offset"], expected_start)
        self.assertEqual([block["file_offset"] for block in result["blocks"]],
                         [expected_start, expected_start + 3])
        self.assertEqual([block["uncompressed_offset"] for block in result["blocks"]], [0, 3])
        self.assertEqual([block["compression"] for block in result["blocks"]], ["none", "none"])

    def test_names_supported_storage_compression_modes(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", 0,
            [(4, 2, 2), (4, 2, 3)],
            [(0, 8, 0, "node.bin")],
        )
        result = unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))
        self.assertEqual([block["compression"] for block in result["blocks"]], ["lz4", "lz4hc"])

    def test_maps_blocks_when_block_info_is_at_the_end(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", unityfs_header.BLOCK_INFO_AT_END,
            [(3, 3, 0), (2, 2, 0)],
            [(0, 5, 0, "node.bin")],
        )
        result = unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))
        self.assertEqual(result["data_offset"], result["header_size"])
        self.assertEqual(result["blocks_payload_end"], result["block_info_offset"])

    def test_aligns_after_start_block_info_for_engine_revisions_that_define_padding(self):
        bundle = unityfs_data_bundle(
            "2019.4.15f1", unityfs_header.BLOCK_INFO_NEED_PADDING_AT_START,
            [(3, 3, 0)], [(0, 3, 0, "node.bin")],
        )
        result = unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))
        self.assertEqual(result["data_offset"] % 16, 0)
        self.assertEqual(result["blocks"][0]["file_offset"], result["data_offset"])

    def test_recognizes_padding_flag_release_thresholds(self):
        for engine in ("2019.4.15f1", "2020.3.34f1", "2021.3.2f1", "2022.1.1f1", "2022.2.0f1"):
            with self.subTest(engine=engine):
                bundle = unityfs_data_bundle(
                    engine, unityfs_header.BLOCK_INFO_NEED_PADDING_AT_START,
                    [(3, 3, 0)], [(0, 3, 0, "node.bin")],
                )
                result = unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))
                self.assertEqual(result["data_offset"] % 16, 0)

    def test_rejects_padding_flag_with_block_info_at_end(self):
        bundle = unityfs_data_bundle(
            "2019.4.15f1", unityfs_header.BLOCK_INFO_AT_END | unityfs_header.BLOCK_INFO_NEED_PADDING_AT_START,
            [(3, 3, 0)], [(0, 3, 0, "node.bin")],
        )
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "block-info-at-end"):
            unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))

    def test_rejects_ambiguous_or_encrypted_legacy_padding_flag(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", unityfs_header.BLOCK_INFO_NEED_PADDING_AT_START,
            [(3, 3, 0)], [(0, 3, 0, "node.bin")],
        )
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "ambiguous or encrypted"):
            unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))

    def test_rejects_blocks_larger_than_the_physical_data_region(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", 0, [(4, 4, 0)], [(0, 4, 0, "node.bin")], payload_size=1
        )
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "block sizes exceed"):
            unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))

    def test_rejects_node_range_outside_the_logical_data(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", 0, [(4, 4, 0)], [(0, 5, 0, "node.bin")]
        )
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "node range"):
            unityfs_header.parse_data_block_layout(io.BytesIO(bundle), len(bundle))

    def test_layout_reads_metadata_without_reading_payload(self):
        bundle = unityfs_data_bundle(
            "2019.4.4f1", 0, [(3, 3, 0)], [(0, 3, 0, "node.bin")]
        )
        stream = CountingStream(bundle)
        result = unityfs_header.parse_data_block_layout(stream, len(bundle))
        self.assertEqual(result["blocks"][0]["compressed_size"], 3)
        self.assertLess(stream.bytes_read, len(bundle))


if __name__ == "__main__":
    unittest.main()
