import json
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))
import asset_manifest


class AssetManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=str(REPOSITORY_ROOT))
        self.base = Path(self.temporary.name)
        self.root = self.base / "authorized-assets"
        self.root.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def test_deterministic_sorted_entries_hashes_and_categories(self):
        (self.root / "B.WAV").write_bytes(b"audio-data")
        (self.root / "a.PNG").write_bytes(b"image-data")
        nested = self.root / "nested"
        nested.mkdir()
        (nested / "unknown.bin").write_bytes(b"opaque-data")

        first = asset_manifest.build_manifest(self.root)
        second = asset_manifest.build_manifest(self.root)

        self.assertEqual(first, second)
        self.assertEqual(first["schema_version"], 2)
        self.assertEqual(first["file_count"], 3)
        self.assertEqual(
            [item["path"] for item in first["files"]],
            ["B.WAV", "a.PNG", "nested/unknown.bin"],
        )
        self.assertEqual(first["files"][0]["extension"], ".wav")
        self.assertEqual(first["files"][0]["format_category"], "audio")
        self.assertEqual(first["files"][1]["format_category"], "image")
        self.assertEqual(first["files"][2]["format_category"], "unknown")
        self.assertEqual(first["files"][0]["depends_on"], [])
        self.assertEqual(
            first["files"][0]["sha256"],
            "9987d4dfea80e5280a1a5e03a72cb35cff47c255c9bcc1fc3bf65373d92660e6",
        )

    def test_canonical_exclusions_skip_files_and_directory_subtrees_case_insensitively(self):
        protected = self.root / "Plugins" / "steam_settings"
        protected.mkdir(parents=True)
        (protected / "private.json").write_text("private-value", encoding="utf-8")
        saves = self.root / "Saves"
        saves.mkdir()
        (saves / "slot.sav").write_bytes(b"private-save")
        (self.root / "Assets").mkdir()
        (self.root / "Assets" / "allowed.bin").write_bytes(b"allowed")

        manifest = asset_manifest.build_manifest(
            self.root, exclusions=["plugins/STEAM_SETTINGS", "saves"]
        )

        self.assertEqual([item["path"] for item in manifest["files"]], ["Assets/allowed.bin"])
        self.assertEqual(manifest["skipped"], [])

    def test_exclusions_reject_noncanonical_and_traversal_paths(self):
        for path in ("../private", "Assets/../private", "/absolute", "Assets\\private"):
            with self.subTest(path=path):
                with self.assertRaisesRegex(asset_manifest.ManifestError, "canonical relative|relative POSIX"):
                    asset_manifest.build_manifest(self.root, exclusions=[path])

    def test_cli_accepts_repeated_exclusions(self):
        private = self.root / "private"
        private.mkdir()
        (private / "config.json").write_text("private", encoding="utf-8")
        (self.root / "allowed.bin").write_bytes(b"allowed")
        output = self.base / "excluded-manifest.json"
        with redirect_stdout(StringIO()):
            result = asset_manifest.main([
                str(self.root), "--exclude", "private", "--exclude", "missing",
                "--output", str(output),
            ])
        self.assertEqual(result, 0)
        manifest = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual([entry["path"] for entry in manifest["files"]], ["allowed.bin"])

    def _write_dependency_map(self, document):
        path = self.base / "dependencies.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def _add_assets(self, *relative_paths):
        for relative in relative_paths:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(relative.encode("utf-8"))

    def test_explicit_dependencies_are_validated_and_deterministic(self):
        self._add_assets(
            "animations/player.json", "audio/hit.wav", "textures/player.png"
        )
        dependency_map = self._write_dependency_map(
            {
                "schema_version": 1,
                "dependencies": [
                    {
                        "asset": "animations/player.json",
                        "depends_on": ["textures/player.png", "audio/hit.wav"],
                    }
                ],
            }
        )

        first = asset_manifest.build_manifest(self.root, dependency_map)
        second = asset_manifest.build_manifest(self.root, dependency_map)
        files = {item["path"]: item for item in first["files"]}

        self.assertEqual(first, second)
        self.assertEqual(first["schema_version"], 2)
        self.assertEqual(
            files["animations/player.json"]["depends_on"],
            ["audio/hit.wav", "textures/player.png"],
        )
        self.assertEqual(files["audio/hit.wav"]["depends_on"], [])

    def test_dependency_map_rejects_unknown_and_noncanonical_paths(self):
        self._add_assets("animations/player.json")
        cases = [
            (
                {"asset": "missing.json", "depends_on": []},
                "not in the inventory",
            ),
            (
                {"asset": "animations/player.json", "depends_on": ["../secret.bin"]},
                "canonical relative POSIX",
            ),
            (
                {"asset": "animations/player.json", "depends_on": ["animations/player.json"]},
                "cannot depend on itself",
            ),
        ]
        for entry, message in cases:
            with self.subTest(message=message):
                dependency_map = self._write_dependency_map(
                    {"schema_version": 1, "dependencies": [entry]}
                )
                with self.assertRaisesRegex(asset_manifest.ManifestError, message):
                    asset_manifest.build_manifest(self.root, dependency_map)

    def test_dependency_map_rejects_duplicates_and_cycles(self):
        self._add_assets("a.json", "b.json")
        duplicate = self._write_dependency_map(
            {
                "schema_version": 1,
                "dependencies": [
                    {"asset": "a.json", "depends_on": ["b.json", "b.json"]}
                ],
            }
        )
        with self.assertRaisesRegex(asset_manifest.ManifestError, "duplicate dependency"):
            asset_manifest.build_manifest(self.root, duplicate)

        cycle = self._write_dependency_map(
            {
                "schema_version": 1,
                "dependencies": [
                    {"asset": "a.json", "depends_on": ["b.json"]},
                    {"asset": "b.json", "depends_on": ["a.json"]},
                ],
            }
        )
        with self.assertRaisesRegex(asset_manifest.ManifestError, "contains a cycle"):
            asset_manifest.build_manifest(self.root, cycle)

    def test_dependency_map_rejects_duplicate_json_keys(self):
        dependency_map = self.base / "duplicate-keys.json"
        dependency_map.write_text(
            '{"schema_version":1,"schema_version":1,"dependencies":[]}',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(asset_manifest.ManifestError, "duplicate JSON key"):
            asset_manifest.build_manifest(self.root, dependency_map)

    def test_dependency_map_is_bounded_and_must_be_outside_root(self):
        inside = self.root / "dependencies.json"
        inside.write_text('{"schema_version":1,"dependencies":[]}', encoding="utf-8")
        with self.assertRaisesRegex(asset_manifest.ManifestError, "outside the scanned root"):
            asset_manifest.build_manifest(self.root, inside)

        oversized = self.base / "oversized-dependencies.json"
        oversized.write_bytes(b"x" * (asset_manifest.MAX_DEPENDENCY_MAP_BYTES + 1))
        with self.assertRaisesRegex(asset_manifest.ManifestError, "exceeds 1 MiB"):
            asset_manifest.build_manifest(self.root, oversized)

    def test_dependency_map_rejects_symlinked_parent_directory(self):
        target_directory = self.base / "real-map-directory"
        target_directory.mkdir()
        dependency_map = target_directory / "dependencies.json"
        dependency_map.write_text('{"schema_version":1,"dependencies":[]}', encoding="utf-8")
        linked_directory = self.base / "linked-map-directory"
        try:
            linked_directory.symlink_to(target_directory, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest("directory symlink creation unavailable: " + str(error))

        with self.assertRaisesRegex(asset_manifest.ManifestError, "symlink or reparse point"):
            asset_manifest.build_manifest(
                self.root, linked_directory / "dependencies.json"
            )

    def test_dependency_map_rejects_reparse_parent_component(self):
        parent = self.base / "reparse-map-parent"
        parent.mkdir()
        dependency_map = parent / "dependencies.json"
        dependency_map.write_text('{"schema_version":1,"dependencies":[]}', encoding="utf-8")
        original_check = asset_manifest._is_reparse_or_link

        def mark_parent_as_reparse(path, metadata):
            return path == parent or original_check(path, metadata)

        with mock.patch.object(
            asset_manifest, "_is_reparse_or_link", side_effect=mark_parent_as_reparse
        ):
            with self.assertRaisesRegex(
                asset_manifest.ManifestError, "symlink or reparse point"
            ):
                asset_manifest.build_manifest(self.root, dependency_map)

    def test_cli_accepts_dependency_map(self):
        self._add_assets("animations/player.json", "textures/player.png")
        dependency_map = self._write_dependency_map(
            {
                "schema_version": 1,
                "dependencies": [
                    {"asset": "animations/player.json", "depends_on": ["textures/player.png"]}
                ],
            }
        )
        output = self.base / "manifest.json"
        with redirect_stdout(StringIO()):
            result = asset_manifest.main(
                [str(self.root), "--dependency-map", str(dependency_map), "--output", str(output)]
            )
        self.assertEqual(result, 0)
        manifest = json.loads(output.read_text(encoding="utf-8"))
        files = {item["path"]: item for item in manifest["files"]}
        self.assertEqual(files["animations/player.json"]["depends_on"], ["textures/player.png"])

    def test_cli_does_not_write_manifest_for_invalid_dependencies(self):
        self._add_assets("animations/player.json")
        dependency_map = self._write_dependency_map(
            {
                "schema_version": 1,
                "dependencies": [
                    {"asset": "animations/player.json", "depends_on": ["missing.png"]}
                ],
            }
        )
        output = self.base / "must-not-exist.json"
        with redirect_stderr(StringIO()):
            with self.assertRaises(SystemExit):
                asset_manifest.main(
                    [str(self.root), "--dependency-map", str(dependency_map), "--output", str(output)]
                )
        self.assertFalse(output.exists())

    def test_manifest_write_is_deterministic_and_outside_root(self):
        (self.root / "one.dat").write_bytes(b"opaque")
        manifest = asset_manifest.build_manifest(self.root)
        first = self.base / "manifest-one.json"
        second = self.base / "manifest-two.json"

        asset_manifest.write_manifest(manifest, first)
        asset_manifest.write_manifest(manifest, second)

        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(json.loads(first.read_text("utf-8")), manifest)
        self.assertEqual(list(self.base.glob("*.tmp")), [])

    def test_symlink_is_reported_and_never_hashed(self):
        external = self.base / "external.bin"
        external.write_bytes(b"outside")
        link = self.root / "external-link.bin"
        try:
            link.symlink_to(external)
        except (OSError, NotImplementedError) as error:
            self.skipTest("symlink creation unavailable: " + str(error))

        manifest = asset_manifest.build_manifest(self.root)

        self.assertEqual(manifest["file_count"], 0)
        self.assertEqual(manifest["skipped_count"], 1)
        self.assertEqual(
            manifest["skipped"],
            [{"path": "external-link.bin", "reason": "link_or_reparse_point"}],
        )

    def test_root_symlink_and_non_directory_are_rejected(self):
        file_path = self.base / "plain.txt"
        file_path.write_text("not a directory", encoding="utf-8")
        with self.assertRaises(asset_manifest.ManifestError):
            asset_manifest.build_manifest(file_path)

        root_link = self.base / "root-link"
        try:
            root_link.symlink_to(self.root, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest("directory symlink creation unavailable: " + str(error))
        with self.assertRaises(asset_manifest.ManifestError):
            asset_manifest.build_manifest(root_link)

    def test_output_inside_root_is_rejected(self):
        with self.assertRaisesRegex(asset_manifest.ManifestError, "outside"):
            asset_manifest._output_path(self.root / "manifest.json", self.root)

    def test_file_change_during_hash_fails_scan(self):
        (self.root / "one.dat").write_bytes(b"opaque")
        with mock.patch.object(
            asset_manifest, "_same_snapshot", side_effect=[True, False]
        ):
            with self.assertRaisesRegex(asset_manifest.ManifestError, "changed"):
                asset_manifest.build_manifest(self.root)

    def test_failed_replace_preserves_existing_manifest_and_cleans_temp(self):
        output = self.base / "manifest.json"
        output.write_bytes(b"previous-manifest")
        with mock.patch.object(asset_manifest.os, "replace", side_effect=OSError("denied")):
            with self.assertRaises(asset_manifest.ManifestError):
                asset_manifest.write_manifest({"schema_version": 1}, output)

        self.assertEqual(output.read_bytes(), b"previous-manifest")
        self.assertEqual(
            [path for path in self.base.iterdir() if path.name.endswith(".tmp")], []
        )


if __name__ == "__main__":
    unittest.main()
