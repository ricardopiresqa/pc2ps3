import contextlib
import io
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))
sys.path.insert(0, str(REPOSITORY_ROOT / "tests"))

import unityfs_extract_nodes as extractor
from test_unityfs_data import unityfs_bundle


def valid_bundle():
    return unityfs_bundle(
        [(2, b"AB", 0), (2, b"CD", 0)],
        [(0, 2, 0, "first.bin"), (2, 2, 0, "folder/second.bin")],
        [b"AB", b"CD"],
    )


class TestUnityFSNodeExportCLI(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(dir=str(REPOSITORY_ROOT / "tmp"))
        self.root = Path(self.temporary_directory.name)
        self.bundle_path = self.root / "synthetic.bundle"
        self.bundle_path.write_bytes(valid_bundle())
        self.output_dir = self.root / "nodes"

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_cli(self, *node_indexes, output_dir=None, bundle_path=None):
        command = [
            sys.executable,
            str(REPOSITORY_ROOT / "tools" / "unityfs_extract_nodes.py"),
            "--bundle",
            str(bundle_path or self.bundle_path),
            "--output-dir",
            str(output_dir or self.output_dir),
        ]
        for node_index in node_indexes:
            command.extend(("--node-index", str(node_index)))
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def test_exports_only_the_selected_node_using_a_generated_name(self):
        result = self.run_cli(1)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output_dir / "node_000001.bin").read_bytes(), b"CD")
        self.assertEqual([path.name for path in self.output_dir.iterdir()], ["node_000001.bin"])

    def test_invalid_and_duplicate_indexes_fail_before_writing(self):
        for label, indexes in (("negative", (-1,)), ("missing", (2,)), ("duplicate", (1, 1))):
            with self.subTest(label=label):
                output_dir = self.root / label
                result = self.run_cli(*indexes, output_dir=output_dir)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output_dir.exists())

    def test_existing_output_is_not_overwritten(self):
        self.output_dir.mkdir()
        target = self.output_dir / "node_000001.bin"
        target.write_bytes(b"keep")

        result = self.run_cli(1)

        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(target.read_bytes(), b"keep")
        self.assertEqual([path.name for path in self.output_dir.iterdir()], [target.name])

    def test_reparse_output_directory_is_rejected(self):
        real_dir = self.root / "real"
        real_dir.mkdir()
        linked_dir = self.root / "linked"
        try:
            os.symlink(real_dir, linked_dir, target_is_directory=True)
        except (NotImplementedError, OSError) as error:
            self.skipTest("directory symlinks are unavailable: {}".format(error))

        result = self.run_cli(1, output_dir=linked_dir)

        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(real_dir.iterdir()), [])

    def test_reparse_point_attributes_are_rejected(self):
        with mock.patch.object(
            Path,
            "lstat",
            return_value=SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=0x400),
        ):
            with self.assertRaisesRegex(ValueError, "reparse point"):
                extractor._validate_output_directory(self.root / "junction")

    def test_reader_and_atomic_publish_errors_leave_no_partial_files(self):
        malformed = unityfs_bundle([(3, b"xxx", 1)], [(0, 3, 0, "bad.bin")], [b"xxx"])
        malformed_path = self.root / "malformed.bundle"
        malformed_path.write_bytes(malformed)
        result = self.run_cli(0, bundle_path=malformed_path)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output_dir.exists())

        publish_one = extractor._publish_one
        publish_count = 0

        def publish_then_fail(temporary, target):
            nonlocal publish_count
            publish_count += 1
            if publish_count == 2:
                raise OSError("publish denied")
            publish_one(temporary, target)

        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            with mock.patch.object(extractor, "_publish_one", side_effect=publish_then_fail):
                exit_code = extractor.main([
                    "--bundle", str(self.bundle_path),
                    "--output-dir", str(self.output_dir),
                    "--node-index", "0",
                    "--node-index", "1",
                ])

        self.assertEqual(exit_code, 1)
        self.assertEqual(list(self.output_dir.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
