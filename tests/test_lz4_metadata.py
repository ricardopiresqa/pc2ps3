import sys
import unittest
from pathlib import Path
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import unityfs_header


class Lz4MetadataDecoderTests(unittest.TestCase):
    def test_decodes_literal_only_block(self):
        self.assertEqual(
            unityfs_header.decompress_lz4_block(b"\x50hello", 5, 5), b"hello"
        )

    def test_decodes_overlapping_match_and_trailing_literals(self):
        block = b"\x32abc\x03\x00\x2012"
        self.assertEqual(
            unityfs_header.decompress_lz4_block(block, 11, 11), b"abcabcabc12"
        )

    def test_decodes_extended_literal_and_match_lengths(self):
        literals = b"\xf0\x01" + (b"a" * 16)
        self.assertEqual(
            unityfs_header.decompress_lz4_block(literals, 16, 16), b"a" * 16
        )
        match = b"\x1fA\x01\x00\x00\x10z"
        self.assertEqual(
            unityfs_header.decompress_lz4_block(match, 21, 21), (b"A" * 20) + b"z"
        )

    def test_accepts_empty_block_only_for_empty_output(self):
        self.assertEqual(unityfs_header.decompress_lz4_block(b"", 0, 0), b"")
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "expected size"):
            unityfs_header.decompress_lz4_block(b"", 1, 1)

    def test_rejects_invalid_input_and_limits_before_decoding(self):
        cases = (
            ("must be bytes", lambda: unityfs_header.decompress_lz4_block("abc", 3, 3)),
            ("expected size", lambda: unityfs_header.decompress_lz4_block(b"", True, 1)),
            ("output limit", lambda: unityfs_header.decompress_lz4_block(b"", 0, True)),
            ("output limit", lambda: unityfs_header.decompress_lz4_block(b"", 1, 0)),
            (
                "supported range",
                lambda: unityfs_header.decompress_lz4_block(
                    b"", 0, unityfs_header.MAX_UNCOMPRESSED_INFO_BYTES + 1
                ),
            ),
        )
        for message, decode in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    decode()
        with patch.object(unityfs_header, "MAX_LZ4_BLOCK_BYTES", 2):
            with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "input size"):
                unityfs_header.decompress_lz4_block(b"\x00\x00\x00", 0, 0)

    def test_rejects_truncated_sequences_and_offsets(self):
        cases = (
            (b"\xf0", 16, "truncated LZ4 literal run length"),
            (b"\x50abc", 5, "truncated LZ4 literal run"),
            (b"\x10A\x01", 5, "truncated LZ4 match offset"),
            (b"\x10A\x00\x00", 5, "offset cannot be zero"),
            (b"\x10A\x02\x00", 5, "offset exceeds produced output"),
            (b"\x1fA\x01\x00", 20, "truncated LZ4 match run length"),
        )
        for block, size, message in cases:
            with self.subTest(block=block):
                with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, message):
                    unityfs_header.decompress_lz4_block(block, size, size)

    def test_rejects_output_overrun_and_underrun(self):
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "exceeds output"):
            unityfs_header.decompress_lz4_block(b"\x30abc", 2, 2)
        with self.assertRaisesRegex(unityfs_header.UnityFSHeaderError, "does not match"):
            unityfs_header.decompress_lz4_block(b"\x20ab", 3, 3)


if __name__ == "__main__":
    unittest.main()
