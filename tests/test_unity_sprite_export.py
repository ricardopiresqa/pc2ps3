import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))
sys.path.insert(0, str(REPOSITORY_ROOT / "tests"))
from test_unityfs_data import unityfs_bundle as build_unityfs_bundle

import build_atlas
import unity_sprite_export as sprite_export
from unity_serialized_file import (
    UnitySerializedObject,
    UnitySpriteRegion,
    UnityTexture2DImageData,
    UnityTexture2DMetadata,
)
from unity_texture_pixels import UnityTexture2DRgba


class UnitySpriteExportTests(unittest.TestCase):
    @staticmethod
    def fixture_directories():
        temporary = tempfile.TemporaryDirectory(prefix="pc2ps3-sprite-export-test-")
        root = Path(temporary.name)
        source_root = root / "source"
        output_parent = root / "exports"
        source_root.mkdir()
        output_parent.mkdir()
        serialized_file = source_root / "private-game-name.assets"
        serialized_file.write_bytes(b"synthetic serialized file")
        return temporary, source_root, output_parent, serialized_file

    @staticmethod
    def install_fixture_patches():
        sprite = UnitySerializedObject(7654321, 0, 1, 0, 213)
        texture = UnitySerializedObject(7654322, 1, 1, 0, 28)
        metadata = {
            "preamble": {"unity_version": "2019.4.4f1", "target_platform": 19},
            "objects": (sprite, texture),
        }
        texture_metadata = UnityTexture2DMetadata(2, 2, 16, 4)
        image = UnityTexture2DImageData(
            2,
            2,
            16,
            4,
            1,
            "inline",
            bytes((255, 0, 0, 1, 0, 255, 0, 2, 0, 0, 255, 3, 255, 255, 0, 4)),
        )
        patches = (
            patch.object(
                sprite_export.unity_serialized_file,
                "parse_serialized_file_header",
                return_value={"file_size": 24, "endian": "little"},
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "parse_serialized_file_metadata_v21",
                return_value=metadata,
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "_read_sprite_render_data_prefix",
                return_value=((0.0, 0.0, 2.0, 2.0), 0, 0, 0, 0, texture.path_id),
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_metadata_prefix",
                return_value=texture_metadata,
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_sprite_region",
                return_value=UnitySpriteRegion(0, 0, 2, 2),
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_image_data",
                return_value=image,
            ),
        )
        return patches

    @contextlib.contextmanager
    def patched_fixture(self):
        with contextlib.ExitStack() as stack:
            for item in self.install_fixture_patches():
                stack.enter_context(item)
            yield

    def test_cli_exports_png_manifest_and_build_atlas_consumes_them(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "sprite-export"
        arguments = [
            "--source-root",
            str(source_root),
            "--serialized-file",
            str(serialized_file),
            "--sprite-index",
            "0",
            "--source-origin",
            "top-left",
            "--output-dir",
            str(output_directory),
        ]
        stdout, stderr = io.StringIO(), io.StringIO()
        with self.patched_fixture(), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = sprite_export.main(arguments)

        self.assertEqual(result, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(json.loads(stdout.getvalue()), {"exported": 1, "width": 2, "height": 2})
        self.assertNotIn("private-game-name", stdout.getvalue())
        self.assertNotIn("765432", stdout.getvalue())

        manifest_path = output_directory / "manifest.json"
        manifest_text = manifest_path.read_text(encoding="utf-8")
        self.assertEqual(
            json.loads(manifest_text),
            {"schema_version": 1, "sprites": [{"id": "sprite-000000", "path": "sprite_000000.png"}]},
        )
        self.assertNotIn("private-game-name", manifest_text)
        self.assertNotIn("765432", manifest_text)

        atlas_output = output_parent / "atlas"
        atlas_output.mkdir()
        build_atlas.build_atlas(output_directory, manifest_path, atlas_output)
        self.assertTrue((atlas_output / "atlas.png").is_file())

    def test_rejects_existing_or_in_project_output_without_partial_files(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        existing_output = output_parent / "existing"
        existing_output.mkdir()
        sentinel = existing_output / "keep.txt"
        sentinel.write_text("preserve", encoding="utf-8")

        with self.patched_fixture():
            with self.assertRaises(sprite_export.SpriteExportError):
                sprite_export.export_sprite(
                    str(serialized_file),
                    source_root=str(source_root),
                    sprite_index=0,
                    source_origin="top-left",
                    output_directory=str(existing_output),
                )
            with self.assertRaises(sprite_export.SpriteExportError):
                sprite_export.export_sprite(
                    str(serialized_file),
                    source_root=str(source_root),
                    sprite_index=0,
                    source_origin="top-left",
                    output_directory=str(source_root / "inside"),
                )

        self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")
        self.assertFalse((source_root / "inside").exists())

    def test_exports_loaded_dxt5_texture_to_png(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "dxt5-export"
        block = bytes((255, 255)) + bytes(6) + b"\x1f\x00\x00\x00" + bytes(4)
        image = UnityTexture2DImageData(2, 2, 16, 12, 1, "loaded", block)

        with (
            self.patched_fixture(),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_metadata_prefix",
                return_value=UnityTexture2DMetadata(2, 2, 16, 12),
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_image_data",
                return_value=image,
            ),
        ):
            result = sprite_export.export_sprite(
                str(serialized_file),
                source_root=str(source_root),
                sprite_index=0,
                source_origin="top-left",
                output_directory=str(output_directory),
            )

        self.assertEqual(result, sprite_export.SpriteExportSummary(2, 2))
        self.assertTrue((output_directory / "sprite_000000.png").is_file())

    def test_exports_loaded_bc7_texture_to_png(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "bc7-export"
        image = UnityTexture2DImageData(2, 2, 16, 25, 1, "loaded", bytes((0x40,)) + bytes(15))

        with (
            self.patched_fixture(),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_metadata_prefix",
                return_value=UnityTexture2DMetadata(2, 2, 16, 25),
            ),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_image_data",
                return_value=image,
            ),
        ):
            result = sprite_export.export_sprite(
                str(serialized_file),
                source_root=str(source_root),
                sprite_index=0,
                source_origin="top-left",
                output_directory=str(output_directory),
            )

        self.assertEqual(result, sprite_export.SpriteExportSummary(2, 2))
        self.assertTrue((output_directory / "sprite_000000.png").is_file())

    def test_unavailable_texture_stream_publishes_no_files(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "missing-stream"
        image = UnityTexture2DImageData(2, 2, 0, 4, 1, "empty", b"")

        with (
            self.patched_fixture(),
            patch.object(
                sprite_export.unity_serialized_file,
                "read_texture2d_image_data",
                return_value=image,
            ),
        ):
            with self.assertRaises(sprite_export.SpriteExportError):
                sprite_export.export_sprite(
                    str(serialized_file),
                    source_root=str(source_root),
                    sprite_index=0,
                    source_origin="top-left",
                    output_directory=str(output_directory),
                )

        self.assertFalse(output_directory.exists())

    def test_failure_is_sanitized_and_publishes_no_directory(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "failed-export"
        stdout, stderr = io.StringIO(), io.StringIO()
        arguments = [
            "--source-root",
            str(source_root),
            "--serialized-file",
            str(serialized_file),
            "--sprite-index",
            "0",
            "--source-origin",
            "top-left",
            "--output-dir",
            str(output_directory),
        ]
        with self.patched_fixture():
            with patch.object(
                sprite_export,
                "decode_texture2d_to_rgba",
                side_effect=ValueError("private-game-name PathID 7654321"),
            ), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = sprite_export.main(arguments)

        self.assertEqual(result, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertEqual(stderr.getvalue(), "sprite export failed\n")
        self.assertNotIn("private-game-name", stderr.getvalue())
        self.assertNotIn("7654321", stderr.getvalue())
        self.assertFalse(output_directory.exists())


    def test_batch_export_is_sorted_and_consumed_by_atlas_builder(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "batch"
        atlas_output = output_parent / "atlas"
        atlas_output.mkdir()
        pngs = {
            0: sprite_export.asset_png.encode_png(
                sprite_export.asset_png.PngImage(1, 1, bytes((255, 0, 0, 255)))
            ),
            1: sprite_export.asset_png.encode_png(
                sprite_export.asset_png.PngImage(1, 1, bytes((0, 255, 0, 255)))
            ),
        }

        def build_export(
            _stream,
            _available_size,
            sprite_index,
            _origin,
            _external_root,
            _parsed_source,
        ):
            return 1, 1, pngs[sprite_index]

        with (
            self.patched_fixture(),
            patch.object(
                sprite_export,
                "_parse_export_metadata",
                wraps=sprite_export._parse_export_metadata,
            ) as parse_metadata,
            patch.object(
                sprite_export, "_build_export_from_stream", side_effect=build_export
            ),
        ):
            result = sprite_export.export_sprites(
                str(serialized_file),
                source_root=str(source_root),
                sprite_indices=(1, 0),
                source_origin="bottom-left",
                output_directory=str(output_directory),
            )

        parse_metadata.assert_called_once()
        self.assertEqual(result.exported, 2)
        self.assertEqual(result.total_png_bytes, len(pngs[0]) + len(pngs[1]))
        self.assertEqual(result.dimensions, (sprite_export.SpriteExportSummary(1, 1),) * 2)
        manifest_path = output_directory / "manifest.json"
        manifest_text = manifest_path.read_text(encoding="utf-8")
        manifest = json.loads(manifest_text)
        self.assertEqual(
            manifest,
            {
                "schema_version": 1,
                "sprites": [
                    {"id": "sprite-000000", "path": "sprite_000000.png"},
                    {"id": "sprite-000001", "path": "sprite_000001.png"},
                ],
            },
        )
        self.assertNotIn("private-game-name", manifest_text)
        build_atlas.build_atlas(output_directory, manifest_path, atlas_output)
        self.assertTrue((atlas_output / "atlas.png").is_file())
        self.assertTrue((atlas_output / "atlas.p2sa").is_file())

    def test_batch_failure_does_not_publish_partial_directory(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "failed-batch"

        def build_export(
            _stream,
            _available_size,
            sprite_index,
            _origin,
            _external_root,
            _parsed_source,
        ):
            if sprite_index == 1:
                raise sprite_export.SpriteExportError("private-game-name pathID 7654321")
            return 1, 1, b"synthetic png bytes"

        with self.patched_fixture(), patch.object(
            sprite_export, "_build_export_from_stream", side_effect=build_export
        ):
            with self.assertRaises(sprite_export.SpriteExportError):
                sprite_export.export_sprites(
                    str(serialized_file),
                    source_root=str(source_root),
                    sprite_indices=(0, 1),
                    source_origin="top-left",
                    output_directory=str(output_directory),
                )

        self.assertFalse(output_directory.exists())
        self.assertEqual(list(output_parent.iterdir()), [])

    def test_batch_rejects_invalid_ordinals_and_aggregate_limit(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "invalid-batch"
        invalid_batches = (
            (),
            (0, 0),
            (-1,),
            (True,),
            tuple(range(sprite_export.MAX_EXPORTED_SPRITES + 1)),
        )
        with self.patched_fixture(), patch.object(
            sprite_export, "_build_export_from_stream"
        ) as build_export:
            for indices in invalid_batches:
                with self.subTest(indices=indices[:3]), self.assertRaises(sprite_export.SpriteExportError):
                    sprite_export.export_sprites(
                        str(serialized_file),
                        source_root=str(source_root),
                        sprite_indices=indices,
                        source_origin="top-left",
                        output_directory=str(output_directory),
                    )
            build_export.assert_not_called()

        with (
            patch.object(sprite_export, "MAX_EXPORTED_BATCH_BYTES", 4),
            patch.object(
                sprite_export,
                "_build_export_from_stream",
                return_value=(1, 1, b"12345"),
            ),
        ):
            with self.assertRaises(sprite_export.SpriteExportError):
                sprite_export.export_sprites(
                    str(serialized_file),
                    source_root=str(source_root),
                    sprite_indices=(0,),
                    source_origin="top-left",
                    output_directory=str(output_directory),
                )
        self.assertFalse(output_directory.exists())
        self.assertEqual(list(output_parent.iterdir()), [])

    def test_cli_accepts_repeated_sprite_index_and_keeps_single_output_shape(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        output_directory = output_parent / "cli-batch"
        png = sprite_export.asset_png.encode_png(
            sprite_export.asset_png.PngImage(1, 1, bytes((1, 2, 3, 255)))
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        arguments = [
            "sprite-export",
            "--source-root",
            str(source_root),
            "--serialized-file",
            str(serialized_file),
            "--sprite-index",
            "1",
            "--sprite-index",
            "0",
            "--source-origin",
            "top-left",
            "--output-dir",
            str(output_directory),
        ]
        with (
            self.patched_fixture(),
            patch.object(
                sprite_export, "_build_export_from_stream", return_value=(1, 1, png)
            ),
            contextlib.redirect_stdout(stdout),
            contextlib.redirect_stderr(stderr),
        ):
            result = sprite_export.main(arguments)
        self.assertEqual(result, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            json.loads(stdout.getvalue()),
            {"exported": 2, "total_png_bytes": len(png) * 2},
        )


    def test_bundle_export_reads_selected_node_in_memory(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        unused_node = b"unselected payload"
        selected_node = serialized_file.read_bytes()
        bundle_bytes = build_unityfs_bundle(
            [(len(unused_node), unused_node, 0), (len(selected_node), selected_node, 0)],
            [
                (0, len(unused_node), 0, "private/unused.assets"),
                (len(unused_node), len(selected_node), 0, "private/internal.assets"),
            ],
            [unused_node, selected_node],
        )
        bundle_path = source_root / "private-game.bundle"
        bundle_path.write_bytes(bundle_bytes)
        output_directory = output_parent / "bundle-export"

        with (
            self.patched_fixture(),
            patch.object(
                sprite_export.unityfs_data,
                "read_node_bytes",
                wraps=sprite_export.unityfs_data.read_node_bytes,
            ) as read_node,
        ):
            result = sprite_export.export_sprites_from_bundle(
                "private-game.bundle",
                source_root=str(source_root),
                node_index=1,
                sprite_indices=(0,),
                source_origin="top-left",
                output_directory=str(output_directory),
            )

        self.assertEqual(result.exported, 1)
        self.assertEqual(result.dimensions, (sprite_export.SpriteExportSummary(2, 2),))
        read_node.assert_called_once()
        self.assertEqual(read_node.call_args.args[2], 1)
        manifest_text = (output_directory / "manifest.json").read_text(encoding="utf-8")
        self.assertNotIn("private-game", manifest_text)
        self.assertNotIn("internal.assets", manifest_text)
        self.assertEqual(
            sorted(path.name for path in source_root.iterdir()),
            ["private-game-name.assets", "private-game.bundle"],
        )

    def test_bundle_export_rejects_bad_node_without_output_or_path_leak(self):
        temporary, source_root, output_parent, _serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        node = b"not a SerializedFile"
        bundle_bytes = build_unityfs_bundle(
            [(len(node), node, 0)], [(0, len(node), 0, "private/internal.assets")], [node]
        )
        bundle_path = source_root / "private-game.bundle"
        bundle_path.write_bytes(bundle_bytes)
        output_directory = output_parent / "invalid-node"
        with self.assertRaises(sprite_export.SpriteExportError) as caught:
            sprite_export.export_sprites_from_bundle(
                str(bundle_path),
                source_root=str(source_root),
                node_index=4,
                sprite_indices=(0,),
                source_origin="top-left",
                output_directory=str(output_directory),
            )
        self.assertNotIn("private-game", str(caught.exception))
        self.assertNotIn("internal.assets", str(caught.exception))
        self.assertFalse(output_directory.exists())
        self.assertEqual(list(output_parent.iterdir()), [])

    def test_bundle_export_rejects_non_serialized_node_atomically(self):
        temporary, source_root, output_parent, _serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        node = b"not a SerializedFile"
        bundle_bytes = build_unityfs_bundle(
            [(len(node), node, 0)], [(0, len(node), 0, "private/internal.assets")], [node]
        )
        bundle_path = source_root / "private-game.bundle"
        bundle_path.write_bytes(bundle_bytes)
        output_directory = output_parent / "malformed-node"
        with self.assertRaises(sprite_export.SpriteExportError) as caught:
            sprite_export.export_sprites_from_bundle(
                str(bundle_path),
                source_root=str(source_root),
                node_index=0,
                sprite_indices=(0,),
                source_origin="top-left",
                output_directory=str(output_directory),
            )
        self.assertNotIn("private-game", str(caught.exception))
        self.assertNotIn("internal.assets", str(caught.exception))
        self.assertFalse(output_directory.exists())
        self.assertEqual(list(output_parent.iterdir()), [])

    def test_cli_rejects_mixed_or_incomplete_bundle_source(self):
        temporary, source_root, output_parent, serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        base = [
            "sprite-export",
            "--source-root",
            str(source_root),
            "--sprite-index",
            "0",
            "--source-origin",
            "top-left",
            "--output-dir",
            str(output_parent / "invalid-cli"),
        ]
        cases = (
            base
            + [
                "--serialized-file",
                str(serialized_file),
                "--bundle",
                "private-game.bundle",
                "--node-index",
                "0",
            ],
            base + ["--bundle", "private-game.bundle"],
            base + ["--node-index", "0", "--serialized-file", str(serialized_file)],
        )
        for arguments in cases:
            with self.subTest(arguments=arguments):
                stdout = io.StringIO()
                stderr = io.StringIO()
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as caught:
                        sprite_export.main(arguments)
                self.assertEqual(caught.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertNotIn("private-game.bundle", stderr.getvalue())
                self.assertFalse((output_parent / "invalid-cli").exists())

    def test_cli_routes_bundle_node_to_batch_exporter(self):
        temporary, source_root, output_parent, _serialized_file = self.fixture_directories()
        self.addCleanup(temporary.cleanup)
        summary = sprite_export.SpriteBatchExportSummary(
            2,
            123,
            (sprite_export.SpriteExportSummary(3, 4), sprite_export.SpriteExportSummary(5, 6)),
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        arguments = [
            "sprite-export",
            "--source-root",
            str(source_root),
            "--bundle",
            "private-game.bundle",
            "--node-index",
            "1",
            "--sprite-index",
            "0",
            "--sprite-index",
            "2",
            "--source-origin",
            "top-left",
            "--output-dir",
            str(output_parent / "bundle-cli"),
        ]
        with (
            patch.object(sprite_export, "export_sprites_from_bundle", return_value=summary) as export,
            contextlib.redirect_stdout(stdout),
            contextlib.redirect_stderr(stderr),
        ):
            result = sprite_export.main(arguments)
        self.assertEqual(result, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(json.loads(stdout.getvalue()), {"exported": 2, "total_png_bytes": 123})
        export.assert_called_once()
        self.assertEqual(export.call_args.args, ("private-game.bundle",))
        self.assertEqual(export.call_args.kwargs["node_index"], 1)
        self.assertEqual(export.call_args.kwargs["sprite_indices"], [0, 2])


if __name__ == "__main__":
    unittest.main()
