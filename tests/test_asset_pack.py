import binascii
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import asset_manifest
import asset_pack


RED_ONE_PIXEL_PNG = bytes(
    [
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
        0x89, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
        0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99,
        0x3D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    ]
)


class AssetPackTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            dir=str(REPOSITORY_ROOT)
        )
        self.base = Path(self.temporary.name)
        self.root = self.base / "source"
        (self.root / "assets").mkdir(parents=True)
        (self.root / "assets" / "red.png").write_bytes(RED_ONE_PIXEL_PNG)
        (self.root / "assets" / "note.md").write_bytes(b"portable asset\n")
        self.manifest_path = self.base / "manifest.json"
        self.output_dir = self.base / "output"
        self.output_dir.mkdir()
        self.refresh_manifest()

    def tearDown(self):
        self.temporary.cleanup()

    def refresh_manifest(self):
        manifest = asset_manifest.build_manifest(self.root)
        asset_manifest.write_manifest(manifest, self.manifest_path)

    def test_package_is_deterministic_and_contains_sorted_entries(self):
        first = self.output_dir / "first.pc2pak"
        second = self.output_dir / "second.pc2pak"
        result = asset_pack.build_pack(self.root, self.manifest_path, first)
        asset_pack.build_pack(self.root, self.manifest_path, second)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(result["entry_count"], 2)
        self.assertEqual(result["sha256"], asset_manifest._hash_regular_file(
            first, first.stat()
        )[1])

        data = first.read_bytes()
        magic, version, flags, count, index_size = asset_pack.HEADER.unpack_from(data)
        self.assertEqual(magic, asset_pack.MAGIC)
        self.assertEqual(version, asset_pack.VERSION)
        self.assertEqual(flags, 0)
        self.assertEqual(count, 2)
        index = data[asset_pack.HEADER.size : asset_pack.HEADER.size + index_size]
        names = []
        cursor = 0
        entries = []
        for _ in range(count):
            name_length, reserved, offset, size, crc32 = asset_pack.ENTRY.unpack_from(
                index, cursor
            )
            cursor += asset_pack.ENTRY.size
            name = index[cursor : cursor + name_length].decode("utf-8")
            cursor += name_length
            self.assertEqual(reserved, 0)
            names.append(name)
            entries.append((offset, size, crc32))
        self.assertEqual(names, ["assets/note.md", "assets/red.png"])
        self.assertEqual(cursor, index_size)
        for name, (offset, size, expected_crc) in zip(names, entries):
            payload = data[offset : offset + size]
            self.assertEqual(len(payload), size)
            self.assertEqual(binascii.crc32(payload) & 0xFFFFFFFF, expected_crc)
            if name == "assets/red.png":
                self.assertEqual(payload, RED_ONE_PIXEL_PNG)

    def test_changed_asset_is_rejected_and_existing_output_is_preserved(self):
        output = self.output_dir / "previous.pc2pak"
        output.write_bytes(b"previous package")
        (self.root / "assets" / "note.md").write_bytes(b"changed\n")
        with self.assertRaisesRegex(asset_pack.PackError, "changed since manifest"):
            asset_pack.build_pack(self.root, self.manifest_path, output)
        self.assertEqual(output.read_bytes(), b"previous package")
        self.assertEqual(list(self.output_dir.glob("*.tmp")), [])
        self.assertEqual(list(self.output_dir.glob(".*.tmp")), [])

    def test_rejects_noncanonical_paths_and_output_inside_root(self):
        self.assertRaisesRegex(asset_pack.PackError, "canonical", asset_pack._canonical_path,
                               "../outside.bin")
        self.assertRaisesRegex(
            asset_pack.PackError,
            "outside (the )?scanned root",
            asset_pack.build_pack,
            self.root,
            self.manifest_path,
            self.root / "bundle.pc2pak",
        )

    def test_rejects_duplicate_manifest_json_keys(self):
        duplicate_manifest = self.base / "duplicate.json"
        duplicate_manifest.write_text(
            '{"schema_version":2,"schema_version":2}', encoding="utf-8"
        )
        with self.assertRaisesRegex(asset_pack.PackError, "duplicate JSON key"):
            asset_pack.build_pack(
                self.root, duplicate_manifest, self.output_dir / "duplicate.pc2pak"
            )

    def test_rejects_source_path_marked_as_link_or_reparse_point(self):
        linked_asset = (self.root / "assets" / "red.png").resolve()
        original_check = asset_manifest._is_reparse_or_link

        def mark_asset_as_link(path, metadata):
            return Path(path).resolve() == linked_asset or original_check(
                path, metadata
            )

        with mock.patch.object(
            asset_manifest, "_is_reparse_or_link", side_effect=mark_asset_as_link
        ):
            with self.assertRaisesRegex(asset_pack.PackError, "link or reparse point"):
                asset_pack.build_pack(
                    self.root, self.manifest_path, self.output_dir / "linked.pc2pak"
                )

    def test_failed_atomic_replace_preserves_previous_package_and_cleans_temps(self):
        output = self.output_dir / "previous.pc2pak"
        output.write_bytes(b"previous package")
        with mock.patch.object(asset_pack.os, "replace", side_effect=OSError("denied")):
            with self.assertRaisesRegex(asset_pack.PackError, "denied"):
                asset_pack.build_pack(self.root, self.manifest_path, output)
        self.assertEqual(output.read_bytes(), b"previous package")
        self.assertEqual(list(self.output_dir.glob(".*.tmp")), [])

    def test_cpp_bundle_readers_consume_python_output(self):
        file_system_reader = os.environ.get("PC2PS3_BUNDLE_FS_TEST")
        texture_reader = os.environ.get("PC2PS3_BUNDLE_TEXTURE_TEST")
        if not file_system_reader or not texture_reader:
            self.skipTest("C++ integration executables are not configured")
        package = self.output_dir / "integration.pc2pak"
        asset_pack.build_pack(self.root, self.manifest_path, package)
        for executable in (file_system_reader, texture_reader):
            result = subprocess.run(
                [executable, str(package)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(
                result.returncode,
                0,
                msg=f"{executable} failed:\n{result.stdout}\n{result.stderr}",
            )


if __name__ == "__main__":
    unittest.main()
