import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools import build_atlas as atlas_builder
from tools.asset_png import PngImage, decode_png, encode_png


_REPO_ROOT = Path(__file__).resolve().parents[1]
_CLI = _REPO_ROOT / "tools" / "build_atlas.py"


class AtlasBuilderCliTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="pc2ps3-atlas-test-", dir=str(_REPO_ROOT / "build")
        )
        self.base = Path(self.temporary.name)
        self.root = self.base / "source"
        self.output = self.base / "output"
        self.output2 = self.base / "output2"
        self.root.mkdir()
        self.output.mkdir()
        self.output2.mkdir()
        self.manifest = self.base / "sprites.json"

    def tearDown(self):
        self.temporary.cleanup()

    def _write_image(self, relative, image):
        path = self.root / Path(*relative.split("/"))
        path.parent.mkdir(parents=True, exist_ok=True)
        encoded = encode_png(image)
        path.write_bytes(encoded)
        return encoded

    def _write_manifest(self, sprites):
        self.manifest.write_text(
            json.dumps({"schema_version": 1, "sprites": sprites}, separators=(",", ":")),
            encoding="utf-8",
        )

    def _run(self, output=None, name="sprites", max_width=16, max_height=16, padding=0):
        output = output or self.output
        return subprocess.run(
            [
                sys.executable,
                str(_CLI),
                str(self.root),
                str(self.manifest),
                "--output-dir",
                str(output),
                "--name",
                name,
                "--max-width",
                str(max_width),
                "--max-height",
                str(max_height),
                "--padding",
                str(padding),
            ],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_cli_composes_rgba_and_sidecar_without_local_paths(self):
        first = PngImage(2, 1, bytes((255, 0, 0, 0, 10, 20, 30, 128)))
        second = PngImage(1, 2, bytes((5, 6, 7, 255, 8, 9, 10, 255)))
        first_bytes = self._write_image("sprites/a.png", first)
        second_bytes = self._write_image("sprites/b.png", second)
        self._write_manifest([
            {"id": "first", "path": "sprites/a.png"},
            {"id": "second", "path": "sprites/b.png"},
        ])

        result = self._run(padding=1)
        self.assertEqual(result.returncode, 0, result.stderr)
        atlas_bytes = (self.output / "sprites.png").read_bytes()
        document_bytes = (self.output / "sprites.json").read_bytes()
        index_bytes = (self.output / "sprites.p2sa").read_bytes()
        image = decode_png(atlas_bytes)
        document = json.loads(document_bytes)
        magic, version, width, height, sprite_count, filename_length = struct.unpack(
            "<8sHIIIH", index_bytes[:24]
        )
        self.assertEqual((magic, version), (b"PC2SATL\0", 1))
        self.assertEqual((width, height, sprite_count), (image.width, image.height, 2))
        cursor = 24
        image_filename = index_bytes[cursor:cursor + filename_length].decode("utf-8")
        cursor += filename_length
        indexed_sprites = {}
        for _ in range(sprite_count):
            id_length, x, y, sprite_width, sprite_height = struct.unpack_from(
                "<HIIII", index_bytes, cursor
            )
            cursor += 18
            identifier = index_bytes[cursor:cursor + id_length].decode("utf-8")
            cursor += id_length
            indexed_sprites[identifier] = (x, y, sprite_width, sprite_height)
        self.assertEqual(image_filename, "sprites.png")
        self.assertEqual(cursor, len(index_bytes))
        self.assertEqual(
            indexed_sprites,
            {
                item["id"]: (item["x"], item["y"], item["width"], item["height"])
                for item in document["sprites"]
            },
        )
        self.assertNotIn(str(self.root).encode("utf-8"), index_bytes)
        self.assertEqual(document["schema_version"], 1)
        self.assertEqual(
            document["manifest_sha256"],
            hashlib.sha256(self.manifest.read_bytes()).hexdigest(),
        )
        self.assertEqual(document["image"]["sha256"], hashlib.sha256(atlas_bytes).hexdigest())
        self.assertEqual(
            {item["id"]: item["source_sha256"] for item in document["sprites"]},
            {
                "first": hashlib.sha256(first_bytes).hexdigest(),
                "second": hashlib.sha256(second_bytes).hexdigest(),
            },
        )
        self.assertNotIn(str(self.root), document_bytes.decode("utf-8"))
        for item in document["sprites"]:
            x, y = item["x"], item["y"]
            if item["id"] == "first":
                self.assertEqual(image.rgba[(y * image.width + x) * 4:(y * image.width + x) * 4 + 8], first.rgba)
            else:
                row0 = (y * image.width + x) * 4
                row1 = ((y + 1) * image.width + x) * 4
                self.assertEqual(image.rgba[row0:row0 + 4], second.rgba[:4])
                self.assertEqual(image.rgba[row1:row1 + 4], second.rgba[4:])

    def test_same_manifest_generates_identical_png_and_json(self):
        source = self._write_image("one.png", PngImage(1, 1, bytes((4, 3, 2, 1))))
        self._write_manifest([{"id": "one", "path": "one.png"}])
        first = self._run(self.output)
        second = self._run(self.output2)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual((self.output / "sprites.png").read_bytes(), (self.output2 / "sprites.png").read_bytes())
        self.assertEqual((self.output / "sprites.json").read_bytes(), (self.output2 / "sprites.json").read_bytes())
        self.assertEqual((self.output / "sprites.p2sa").read_bytes(), (self.output2 / "sprites.p2sa").read_bytes())
        self.assertEqual(hashlib.sha256(source).hexdigest(), json.loads((self.output / "sprites.json").read_text(encoding="utf-8"))["sprites"][0]["source_sha256"])

    def test_invalid_manifest_preserves_existing_outputs(self):
        self._write_manifest([{"id": "escape", "path": "../outside.png"}])
        old_png = b"previous PNG"
        old_json = b"previous JSON"
        old_index = b"previous index"
        (self.output / "sprites.png").write_bytes(old_png)
        (self.output / "sprites.json").write_bytes(old_json)
        (self.output / "sprites.p2sa").write_bytes(old_index)

        result = self._run()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.output / "sprites.png").read_bytes(), old_png)
        self.assertEqual((self.output / "sprites.json").read_bytes(), old_json)
        self.assertEqual((self.output / "sprites.p2sa").read_bytes(), old_index)
        self.assertEqual(list(self.output.glob(".pc2ps3-atlas-*.tmp")), [])

    def test_invalid_png_and_duplicate_json_key_fail_without_outputs(self):
        (self.root / "bad.png").write_bytes(b"not a PNG")
        self._write_manifest([{"id": "bad", "path": "bad.png"}])
        old_png = b"old atlas"
        old_json = b"old metadata"
        (self.output / "sprites.png").write_bytes(old_png)
        (self.output / "sprites.json").write_bytes(old_json)
        result = self._run()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.output / "sprites.png").read_bytes(), old_png)
        self.assertEqual((self.output / "sprites.json").read_bytes(), old_json)

        self.manifest.write_text(
            '{"schema_version":1,"schema_version":1,"sprites":[]}', encoding="utf-8"
        )
        duplicate = self._run()
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertEqual((self.output / "sprites.png").read_bytes(), old_png)

    def test_sprite_that_exceeds_requested_bounds_preserves_outputs(self):
        self._write_image("large.png", PngImage(2, 2, bytes((1, 2, 3, 255)) * 4))
        self._write_manifest([{"id": "large", "path": "large.png"}])
        old_png = b"old atlas"
        old_json = b"old metadata"
        (self.output / "sprites.png").write_bytes(old_png)
        (self.output / "sprites.json").write_bytes(old_json)

        result = self._run(max_width=1, max_height=1)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.output / "sprites.png").read_bytes(), old_png)
        self.assertEqual((self.output / "sprites.json").read_bytes(), old_json)

    def test_rejects_linked_sprite_when_symlink_creation_is_available(self):
        original = self._write_image("original.png", PngImage(1, 1, bytes((1, 2, 3, 4))))
        link = self.root / "linked.png"
        try:
            link.symlink_to(self.root / "original.png")
        except (OSError, NotImplementedError) as error:
            self.skipTest("symlink creation is unavailable: {}".format(type(error).__name__))
        self.assertTrue(original)
        self._write_manifest([{"id": "linked", "path": "linked.png"}])
        result = self._run()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / "sprites.png").exists())

    def test_rejects_reparse_point_reported_by_filesystem(self):
        self._write_image("one.png", PngImage(1, 1, bytes((1, 2, 3, 255))))
        self._write_manifest([{"id": "one", "path": "one.png"}])
        actual_check = atlas_builder._is_link_or_reparse

        def report_reparse(path, metadata):
            if path.name == "one.png":
                return True
            return actual_check(path, metadata)

        with mock.patch.object(atlas_builder, "_is_link_or_reparse", side_effect=report_reparse):
            with self.assertRaises(atlas_builder.AtlasBuildError):
                atlas_builder.build_atlas(self.root, self.manifest, self.output)
        self.assertFalse((self.output / "atlas.png").exists())

    def test_rejects_output_inside_source_root(self):
        self._write_image("one.png", PngImage(1, 1, bytes((1, 2, 3, 255))))
        self._write_manifest([{"id": "one", "path": "one.png"}])
        output_inside = self.root / "generated"
        output_inside.mkdir()
        result = self._run(output_inside)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(output_inside.iterdir()), [])

    def test_publish_failure_restores_existing_sidecars(self):
        self._write_image("one.png", PngImage(1, 1, bytes((1, 2, 3, 255))))
        self._write_manifest([{"id": "one", "path": "one.png"}])
        atlas_builder.build_atlas(
            self.root, self.manifest, self.output, name="sprites"
        )
        outputs = tuple(
            self.output / name
            for name in ("sprites.png", "sprites.json", "sprites.p2sa")
        )
        before = {path: path.read_bytes() for path in outputs}

        self._write_image("two.png", PngImage(1, 1, bytes((9, 8, 7, 255))))
        self._write_manifest([{"id": "two", "path": "two.png"}])
        original_replace = atlas_builder.os.replace
        calls = 0

        def fail_index_publish(source, destination):
            nonlocal calls
            calls += 1
            if calls == 6:
                raise OSError("injected publish failure")
            return original_replace(source, destination)

        with mock.patch.object(
            atlas_builder.os, "replace", side_effect=fail_index_publish
        ):
            with self.assertRaises(OSError):
                atlas_builder.build_atlas(
                    self.root, self.manifest, self.output, name="sprites"
                )

        self.assertEqual(calls, 9)
        self.assertEqual({path: path.read_bytes() for path in outputs}, before)
        self.assertEqual(list(self.output.glob(".pc2ps3-atlas-*.tmp")), [])

    def test_publish_failure_removes_new_sidecars_without_previous_outputs(self):
        self._write_image("one.png", PngImage(1, 1, bytes((1, 2, 3, 255))))
        self._write_manifest([{"id": "one", "path": "one.png"}])
        original_replace = atlas_builder.os.replace
        calls = 0

        def fail_index_publish(source, destination):
            nonlocal calls
            calls += 1
            if calls == 3:
                raise OSError("injected publish failure")
            return original_replace(source, destination)

        with mock.patch.object(
            atlas_builder.os, "replace", side_effect=fail_index_publish
        ):
            with self.assertRaises(OSError):
                atlas_builder.build_atlas(
                    self.root, self.manifest, self.output, name="sprites"
                )

        self.assertEqual(calls, 3)
        self.assertEqual(list(self.output.iterdir()), [])

    def test_duplicate_ids_and_noncanonical_paths_are_rejected(self):
        self._write_image("one.png", PngImage(1, 1, bytes((1, 2, 3, 255))))
        self._write_image("two.png", PngImage(1, 1, bytes((4, 5, 6, 255))))
        invalid_manifests = (
            [
                {"id": "same", "path": "one.png"},
                {"id": "same", "path": "two.png"},
            ],
            [{"id": "absolute", "path": "/one.png"}],
            [{"id": "backslash", "path": "folder\\one.png"}],
            [{"id": "traversal", "path": "folder/../one.png"}],
        )
        for sprites in invalid_manifests:
            with self.subTest(sprites=sprites):
                self._write_manifest(sprites)
                result = self._run()
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.output / "sprites.png").exists())
                self.assertFalse((self.output / "sprites.json").exists())


if __name__ == "__main__":
    unittest.main()
