import io
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import unityfs_header


def lz4_literals(value):
    length = len(value)
    token = min(length, 15) << 4
    encoded = bytearray([token])
    if length >= 15:
        remainder = length - 15
        while remainder >= 255:
            encoded.append(255)
            remainder -= 255
        encoded.append(remainder)
    encoded.extend(value)
    return bytes(encoded)


def make_block_info(blocks=None, nodes=None):
    blocks = blocks if blocks is not None else [(8, 8, 0)]
    nodes = nodes if nodes is not None else [(0, 8, 0, "assets/scene.assets")]
    value = bytearray(b"\x00" * 16)
    value.extend(struct.pack(">i", len(blocks)))
    for uncompressed_size, compressed_size, flags in blocks:
        value.extend(struct.pack(">IIH", uncompressed_size, compressed_size, flags))
    value.extend(struct.pack(">i", len(nodes)))
    for offset, size, flags, path in nodes:
        value.extend(struct.pack(">qqI", offset, size, flags))
        value.extend(path.encode("utf-8") + b"\x00")
    return bytes(value)


def make_bundle(block_info, compression=0, at_end=False, flags=0, asset_data=b"12345678"):
    if compression == 0:
        encoded_info = block_info
    else:
        encoded_info = lz4_literals(block_info)
    archive_flags = flags | compression | (0x80 if at_end else 0)
    prefix = b"UnityFS\x00" + struct.pack(">I", 6) + b"5.x.x\x00" + b"2019.4.4f1\x00"
    header_size = len(prefix) + 20
    total_size = header_size + len(encoded_info) + len(asset_data)
    header = prefix + struct.pack(">QIII", total_size, len(encoded_info), len(block_info), archive_flags)
    body = asset_data + encoded_info if at_end else encoded_info + asset_data
    return header + body, header_size, len(encoded_info)


class CountingStream(io.BytesIO):
    def __init__(self, value):
        super().__init__(value)
        self.bytes_read = 0

    def read(self, size=-1):
        value = super().read(size)
        self.bytes_read += len(value)
        return value


class UnityFSDirectoryInfoTests(unittest.TestCase):
    def test_parses_uncompressed_info_at_header_without_reading_assets(self):
        info = make_block_info()
        bundle, header_size, info_size = make_bundle(info)
        stream = CountingStream(bundle)

        result = unityfs_header.parse_directory_info(stream, len(bundle))

        self.assertEqual(result["md5"], "00" * 16)
        self.assertEqual(result["block_info_offset"], header_size)
        self.assertEqual(result["nodes"], [{
            "offset": 0, "size": 8, "flags": 0, "path": "assets/scene.assets"
        }])
        self.assertEqual(stream.bytes_read, header_size + info_size)

        lz4_bundle, _, _ = make_bundle(info, compression=2, flags=0x40)
        self.assertEqual(
            unityfs_header.parse_directory_info(io.BytesIO(lz4_bundle), len(lz4_bundle))["compression"],
            "lz4",
        )

    def test_parses_lz4hc_info_at_end_without_reading_assets(self):
        info = make_block_info(blocks=[(16, 8, 0x42)], nodes=[(2, 8, 3, "data/texture.bin")])
        bundle, header_size, info_size = make_bundle(info, compression=3, at_end=True, flags=0x40)
        stream = CountingStream(bundle)

        result = unityfs_header.parse_directory_info(stream, len(bundle))

        self.assertEqual(result["compression"], "lz4hc")
        self.assertEqual(result["block_info_offset"], len(bundle) - info_size)
        self.assertEqual(result["uncompressed_block_size_total"], 16)
        self.assertEqual(stream.bytes_read, header_size + info_size)

    def test_rejects_bad_node_paths_ranges_duplicates_and_table_sizes(self):
        cases = (
            ([(0, 1, 0, "../escape")], [(8, 8, 0)], "canonical relative"),
            ([(0, 8, 0, "same"), (0, 8, 0, "same")], [(8, 8, 0)], "duplicate"),
            ([(7, 2, 0, "overflow")], [(8, 8, 0)], "node range"),
            ([((1 << 63) - 1, 1, 0, "int64-overflow")], [(8, 8, 0)], "node range"),
            ([(0, 8, 0, "assets/a")], [(8, 9, 2)], "asset-data range"),
        )
        for nodes, blocks, message in cases:
            with self.subTest(message=message):
                info = make_block_info(blocks=blocks, nodes=nodes)
                bundle, _, _ = make_bundle(info)
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

    def test_rejects_truncated_info_and_invalid_counts(self):
        cases = (
            (b"\x00" * 20, "truncated UnityFS node count"),
            (b"\x00" * 16 + struct.pack(">i", -1), "block count"),
            (b"\x00" * 16 + struct.pack(">i", 65537), "block count"),
            (b"\x00" * 16 + struct.pack(">i", 0) + struct.pack(">i", 1000001), "node count"),
            (b"\x00" * 16 + struct.pack(">ii", 0, 1), "truncated UnityFS node table"),
        )
        for info, message in cases:
            with self.subTest(message=message):
                bundle, _, _ = make_bundle(info)
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

    def test_rejects_unsupported_archive_block_and_info_compression(self):
        info = make_block_info()
        cases = (
            (0, False, 0x100, "unsupported flags"),
            (1, False, 0, "unsupported UnityFS block-info compression"),
        )
        for compression, at_end, flags, message in cases:
            with self.subTest(message=message):
                bundle, _, _ = make_bundle(info, compression=compression, at_end=at_end, flags=flags)
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

        bad_block_flags = make_block_info(blocks=[(8, 8, 0x80)])
        bundle, _, _ = make_bundle(bad_block_flags)
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "unsupported flags"):
            unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

        bad_block_compression = make_block_info(blocks=[(8, 8, 4)])
        bundle, _, _ = make_bundle(bad_block_compression)
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "unsupported compression"):
            unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

    def test_rejects_noncanonical_node_paths_and_oversized_compressed_range(self):
        for path in ("/absolute", "C:/drive", "dir\\file", "dir/../file", "dir//file"):
            with self.subTest(path=path):
                info = make_block_info(nodes=[(0, 8, 0, path)])
                bundle, _, _ = make_bundle(info)
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "canonical relative"):
                    unityfs_header.parse_directory_info(io.BytesIO(bundle), len(bundle))

        info = make_block_info()
        bundle, header_size, _ = make_bundle(info)
        stream = CountingStream(bundle)
        with patch.object(unityfs_header, "MAX_COMPRESSED_INFO_BYTES", 1):
            with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "reader limit"):
                unityfs_header.parse_directory_info(stream, len(bundle))
        self.assertEqual(stream.bytes_read, header_size)

        with patch.object(unityfs_header, "MAX_DIRECTORY_INFO_BYTES", 1):
            with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "parser limit"):
                unityfs_header.parse_directory_info(stream, len(bundle))
        self.assertEqual(stream.bytes_read, header_size * 2)


if __name__ == "__main__":
    unittest.main()
