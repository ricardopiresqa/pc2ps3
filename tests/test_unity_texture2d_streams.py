import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))
sys.path.insert(0, str(REPOSITORY_ROOT / "tests"))

from test_unity_serialized_file import TestUnityTexture2DImageData as _FixtureClass
from unity_serialized_file import UnitySerializedFileError, read_texture2d_image_data
from unity_texture_pixels import (
    UnityTexturePixelsError,
    decode_loaded_texture2d_to_rgba,
)

_texture_fixture = _FixtureClass().fixture
del _FixtureClass


class UnityTexture2DExternalStreamTests(unittest.TestCase):
    def make_external_texture(self, root, path, payload, *, offset=0, format_id=3):
        path_bytes = path if isinstance(path, bytes) else path.encode("utf-8")
        block_compressed = format_id in (12, 25)
        dimensions = 4 if block_compressed else (len(payload) if format_id == 1 else 1)
        height = 4 if block_compressed else 1
        stream_size = 16 if block_compressed else len(payload)
        stream, header, preamble, record = _texture_fixture(
            texture_format=format_id,
            width=dimensions,
            height=height,
            image_data=b"",
            complete_image_size=stream_size,
            mip_count=1,
            stream_offset=offset,
            stream_size=stream_size,
            stream_path=path_bytes,
        )
        return read_texture2d_image_data(
            stream,
            header,
            preamble,
            record,
            external_data_root=root,
        )

    def test_reads_exact_external_range_and_keeps_path_private(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "sub").mkdir()
            (root / "sub" / "data.resS").write_bytes(b"prefix\x01\x02\x03suffix")

            image = self.make_external_texture(
                root, "sub/data.resS", b"\x01\x02\x03", offset=6
            )

        self.assertEqual(image.storage, "loaded")
        self.assertEqual(image.image_data, b"\x01\x02\x03")
        self.assertFalse(hasattr(image, "stream_path"))
        self.assertNotIn("data.resS", repr(image))

    def test_reads_archive_virtual_path_as_confined_relative_stream_path(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "archive-id").mkdir()
            (root / "archive-id" / "data.resS").write_bytes(b"prefix\x01\x02\x03suffix")

            image = self.make_external_texture(
                root,
                "archive:/archive-id/data.resS",
                b"\x01\x02\x03",
                offset=6,
            )

            self.assertEqual(image.storage, "loaded")
            self.assertEqual(image.image_data, b"\x01\x02\x03")
            self.assertNotIn("archive-id", repr(image))

    def test_loaded_alpha8_flows_into_existing_rgba_decoder(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "alpha.resS").write_bytes(bytes((0, 128)))
            image = self.make_external_texture(
                root, "alpha.resS", bytes((0, 128)), format_id=1
            )

        decoded = decode_loaded_texture2d_to_rgba(image)

        self.assertEqual(
            decoded.pixels,
            bytes((255, 255, 255, 0, 255, 255, 255, 128)),
        )

    def test_loaded_rgb24_and_rgba32_flow_into_existing_rgba_decoder(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "rgb.resS").write_bytes(bytes((1, 2, 3)))
            (root / "rgba.resS").write_bytes(bytes((4, 5, 6, 7)))
            rgb = self.make_external_texture(root, "rgb.resS", bytes((1, 2, 3)))
            rgba = self.make_external_texture(
                root, "rgba.resS", bytes((4, 5, 6, 7)), format_id=4
            )

        self.assertEqual(decode_loaded_texture2d_to_rgba(rgb).pixels, bytes((1, 2, 3, 255)))
        self.assertEqual(decode_loaded_texture2d_to_rgba(rgba).pixels, bytes((4, 5, 6, 7)))

    def test_default_reader_keeps_external_payload_metadata_only(self):
        stream, header, preamble, record = _texture_fixture(
            texture_format=3,
            width=1,
            height=1,
            image_data=b"",
            complete_image_size=3,
            mip_count=1,
            stream_size=3,
            stream_path=b"missing.resS",
        )

        image = read_texture2d_image_data(stream, header, preamble, record)

        self.assertEqual((image.storage, image.image_data), ("external", None))
        self.assertNotIn("missing.resS", repr(image))
        with self.assertRaisesRegex(UnityTexturePixelsError, "loaded.*required"):
            decode_loaded_texture2d_to_rgba(image)

    def test_traversal_absolute_drive_uri_nul_and_invalid_utf8_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            malicious_paths = (
                b"../escape.resS",
                b"sub/../escape.resS",
                b"..\\escape.resS",
                b"/absolute.resS",
                b"C:\\absolute.resS",
                b"archive:/CAB-resS",
            b"archive:/CAB/../escape.resS",
            b"archive:/../escape.resS",
                b"bad\x00path.resS",
                b"\xff.resS",
                b"sub//data.resS",
                b"./data.resS",
            )
            for path in malicious_paths:
                with self.subTest(path=path):
                    with self.assertRaises(UnitySerializedFileError) as caught:
                        self.make_external_texture(root, path, b"123")
                    self.assertNotIn(repr(path), str(caught.exception))

    def test_missing_files_and_ranges_beyond_file_are_rejected_without_path(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "short.resS").write_bytes(b"1234")
            cases = (
                ("missing.resS", 0),
                ("short.resS", 2),
            )
            for path, offset in cases:
                with self.subTest(path=path, offset=offset):
                    with self.assertRaises(UnitySerializedFileError) as caught:
                        self.make_external_texture(
                            root, path, b"123", offset=offset
                        )
                    self.assertNotIn(path, str(caught.exception))

    def test_external_root_must_be_explicitly_absolute(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "alpha.resS").write_bytes(b"x")
            relative_root = root.relative_to(REPOSITORY_ROOT)

            with self.assertRaisesRegex(UnitySerializedFileError, "root must be absolute"):
                self.make_external_texture(relative_root, "alpha.resS", b"x", format_id=1)

        with self.assertRaisesRegex(UnitySerializedFileError, "root must be a local"):
            self.make_external_texture(r"\\server\share", "alpha.resS", b"x", format_id=1)

    def test_resolved_symlink_target_cannot_escape_explicit_asset_root(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            base = Path(temporary)
            root = base / "assets"
            root.mkdir()
            outside = base / "outside.resS"
            outside.write_bytes(b"123")
            resolved_root = root.resolve()
            resolved_outside = outside.resolve()
            with patch(
                "unity_serialized_file.Path.resolve",
                side_effect=(resolved_root, resolved_outside),
            ), patch("unity_serialized_file.Path.is_symlink", return_value=False):
                with self.assertRaisesRegex(
                    UnitySerializedFileError, "outside the asset root"
                ):
                    self.make_external_texture(root, "escape.resS", b"123")

    def test_link_components_are_rejected_before_resolution(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            with patch("unity_serialized_file.Path.is_symlink", return_value=True), patch(
                "unity_serialized_file.Path.resolve", side_effect=(root,)
            ) as resolve:
                with self.assertRaisesRegex(UnitySerializedFileError, "contains a link"):
                    self.make_external_texture(root, "link.resS", b"123")
            self.assertEqual(resolve.call_count, 1)

    def test_loaded_bc7_is_decoded(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            block = bytes((0x40,)) + bytes(15)
            (root / "bc7.resS").write_bytes(block)
            image = self.make_external_texture(
                root, "bc7.resS", block, format_id=25
            )

        self.assertEqual(image.storage, "loaded")
        decoded = decode_loaded_texture2d_to_rgba(image)
        self.assertEqual(decoded.pixels, bytes(4 * 4 * 4))


    def test_dxt5_metadata_size_mismatch_is_rejected_before_stream_read(self):
        stream, header, preamble, record = _texture_fixture(
            texture_format=12,
            width=5,
            height=3,
            image_data=b"",
            complete_image_size=16,
            mip_count=1,
            stream_size=16,
            stream_path=b"texture.resS",
        )
        with self.assertRaisesRegex(UnitySerializedFileError, "size does not match"):
            read_texture2d_image_data(stream, header, preamble, record)

    def test_external_dxt5_stream_is_loaded_and_decoded(self):
        alpha = bytes((255, 255)) + bytes(6)
        color = (0x001F).to_bytes(2, "little") + (0).to_bytes(2, "little") + bytes(4)
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "tmp") as temporary:
            root = Path(temporary)
            (root / "texture.resS").write_bytes(alpha + color)
            image = self.make_external_texture(root, "texture.resS", alpha + color, format_id=12)
            decoded = decode_loaded_texture2d_to_rgba(image)
        self.assertEqual(image.storage, "loaded")
        self.assertEqual(decoded.pixels, bytes((0, 0, 255, 255)) * 16)


if __name__ == "__main__":
    unittest.main()
