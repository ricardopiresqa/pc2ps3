import io
import struct
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import unityfs_data
import unityfs_header


def lz4_literals(value):
    if len(value) >= 15:
        raise ValueError("test fixture only supports short literals")
    return bytes((len(value) << 4,)) + value


def unityfs_bundle(blocks, nodes, payload):
    block_info = bytearray(16)
    block_info.extend(struct.pack(">i", len(blocks)))
    for uncompressed_size, compressed, flags in blocks:
        block_info.extend(struct.pack(">IIH", uncompressed_size, len(compressed), flags))
    block_info.extend(struct.pack(">i", len(nodes)))
    for offset, size, flags, path in nodes:
        block_info.extend(struct.pack(">qqI", offset, size, flags))
        block_info.extend(path.encode("utf-8") + b"\0")

    fields = b"UnityFS\0" + struct.pack(">I", 6) + b"5.x.x\0" + b"2019.4.4f1\0"
    total_size = len(fields) + 20 + len(block_info) + sum(len(item) for item in payload)
    header = fields + struct.pack(">QIII", total_size, len(block_info), len(block_info), 0)
    return header + bytes(block_info) + b"".join(payload)


class CountingStream(io.BytesIO):
    def __init__(self, value):
        super().__init__(value)
        self.read_positions = []

    def read(self, size=-1):
        self.read_positions.append(self.tell())
        return super().read(size)


class UnityFSNodeReaderTests(unittest.TestCase):
    def setUp(self):
        self.raw = b"AB"
        self.lz4 = lz4_literals(b"CDE")
        self.lz4hc = lz4_literals(b"FG")
        self.blocks = [
            (2, self.raw, 0),
            (3, self.lz4, 2),
            (2, self.lz4hc, 3),
        ]
        self.bundle = unityfs_bundle(
            self.blocks, [(0, 7, 0, "node.bin")], [self.raw, self.lz4, self.lz4hc]
        )
        self.stream = CountingStream(self.bundle)
        self.layout = unityfs_header.parse_data_block_layout(self.stream, len(self.bundle))

    def test_reads_none_lz4_and_lz4hc_in_logical_order(self):
        self.assertEqual(unityfs_data.read_node_bytes(self.stream, self.layout, 0), b"ABCDEFG")

    def test_reads_node_across_block_boundary(self):
        bundle = unityfs_bundle(
            [(2, b"AB", 0), (4, lz4_literals(b"CDEF"), 2)],
            [(1, 4, 0, "cross.bin")],
            [b"AB", lz4_literals(b"CDEF")],
        )
        stream = io.BytesIO(bundle)
        layout = unityfs_header.parse_data_block_layout(stream, len(bundle))
        self.assertEqual(unityfs_data.read_node_bytes(stream, layout, 0), b"BCDE")

    def test_does_not_read_blocks_outside_the_selected_node(self):
        bundle = unityfs_bundle(
            self.blocks,
            [(0, 2, 0, "first.bin"), (2, 3, 0, "second.bin"), (5, 2, 0, "third.bin")],
            [self.raw, self.lz4, self.lz4hc],
        )
        stream = CountingStream(bundle)
        layout = unityfs_header.parse_data_block_layout(stream, len(bundle))
        stream.read_positions = []
        self.assertEqual(unityfs_data.read_node_bytes(stream, layout, 1), b"CDE")
        self.assertEqual(stream.read_positions, [layout["blocks"][1]["file_offset"]])

    def test_rejects_lzma_without_reading_the_block(self):
        bundle = unityfs_bundle([(3, b"xxx", 1)], [(0, 3, 0, "lzma.bin")], [b"xxx"])
        stream = CountingStream(bundle)
        layout = unityfs_header.parse_data_block_layout(stream, len(bundle))
        stream.read_positions = []
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "unsupported"):
            unityfs_data.read_node_bytes(stream, layout, 0)
        self.assertEqual(stream.read_positions, [])

    def test_rejects_unknown_or_encrypted_compression(self):
        malformed = dict(self.layout)
        malformed["blocks"] = [dict(block, compression="encrypted") for block in self.layout["blocks"]]
        self.stream.read_positions = []
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "unsupported"):
            unityfs_data.read_node_bytes(self.stream, malformed, 0)
        self.assertEqual(self.stream.read_positions, [])

    def test_rejects_short_reads_and_malformed_lz4(self):
        truncated = io.BytesIO(self.bundle[:-1])
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "truncated"):
            unityfs_data.read_node_bytes(truncated, self.layout, 0)

        bad_bundle = unityfs_bundle([(3, b"\x30AB", 2)], [(0, 3, 0, "bad.bin")], [b"\x30AB"])
        bad_stream = io.BytesIO(bad_bundle)
        bad_layout = unityfs_header.parse_data_block_layout(bad_stream, len(bad_bundle))
        with self.assertRaises(unityfs_header.UnityFSHeaderError):
            unityfs_data.read_node_bytes(bad_stream, bad_layout, 0)

    def test_rejects_invalid_index_and_output_limit_before_reading(self):
        self.stream.read_positions = []
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "index"):
            unityfs_data.read_node_bytes(self.stream, self.layout, True)
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "output limit"):
            unityfs_data.read_node_bytes(self.stream, self.layout, 0, max_output_bytes=6)
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "output limit"):
            unityfs_data.read_node_bytes(
                self.stream, self.layout, 0,
                max_output_bytes=unityfs_data.MAX_NODE_OUTPUT_BYTES + 1,
            )
        self.assertEqual(self.stream.read_positions, [])

    def test_rejects_node_range_outside_the_layout(self):
        malformed = dict(self.layout)
        malformed["nodes"] = [{"offset": 5, "size": 3, "flags": 0, "path": "bad.bin"}]
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "node range"):
            unityfs_data.read_node_bytes(self.stream, malformed, 0)


if __name__ == "__main__":
    unittest.main()
