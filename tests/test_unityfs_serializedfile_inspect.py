import io
import io
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
CLI_PATH = REPOSITORY_ROOT / "tools" / "unityfs_serializedfile_inspect.py"
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import unityfs_serializedfile_inspect as inspector


def serialized_file_v21(
    user_information=b"QA_SECRET_INFO",
    unity_version=b"2019.4.4f1",
    has_type_tree=True,
    object_class_id=1,
    object_payload=None,
):
    metadata = bytearray(unity_version + b"\0")
    metadata.extend(struct.pack("<i", 19))
    metadata.append(1 if has_type_tree else 0)
    string_buffer = b"Root\0Node\0"
    type_tree_node = struct.pack("<HBBIIiiiQ", 1, 0, 0, 0, 5, 0, 0, 0, 0)
    type_tree = struct.pack("<ii", 1, len(string_buffer)) + type_tree_node + string_buffer
    metadata.extend(struct.pack("<i", 1))
    metadata.extend(struct.pack("<iBh", object_class_id, 0, -1))
    metadata.extend(b"\xA5" * 16)
    if has_type_tree:
        metadata.extend(type_tree)
        metadata.extend(struct.pack("<i", 0))
    metadata.extend(struct.pack("<i", 1))
    metadata.extend(b"\0" * (-(20 + len(metadata)) % 4))
    object_size = len(object_payload) if object_payload is not None else 0
    metadata.extend(struct.pack("<qIIi", 7, 0, object_size, 0))
    metadata.extend(struct.pack("<iii", 0, 0, 0))
    metadata.extend(user_information + b"\0")
    data = b"DATA" if object_payload is None else object_payload
    metadata_end = 20 + len(metadata)
    data_padding = b"\0" * (-metadata_end % 4)
    data_offset = metadata_end + len(data_padding)
    file_size = data_offset + len(data)
    header = struct.pack(
        ">IIII", len(metadata), file_size, 21, data_offset
    ) + bytes((0, 0, 0, 0))
    return header + metadata + data_padding + data


def texture2d_payload(name=b"private-texture-x", pixels=b"PIXEL_SECRET"):
    payload = bytearray(struct.pack("<i", len(name)) + name)
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(struct.pack("<iB", 0, 1))
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(struct.pack("<iiii", 128, 64, 8192, 4))
    payload.extend(pixels)
    return bytes(payload)


def texture2d_image_payload(
    texture_format,
    width,
    height,
    image_data=b"",
    stream_offset=0,
    stream_size=0,
    stream_path=b"",
):
    payload = bytearray(struct.pack("<i", 7) + b"private")
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(struct.pack("<iB", 0, 1))
    payload.extend(b"\0" * (-len(payload) % 4))
    complete_image_size = len(image_data) if image_data else stream_size
    payload.extend(struct.pack("<iiii", width, height, complete_image_size, texture_format))
    payload.extend(struct.pack("<i", 1))
    payload.extend(b"\0" * 48)
    payload.extend(struct.pack("<i", len(image_data)))
    payload.extend(image_data)
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(struct.pack("<IIi", stream_offset, stream_size, len(stream_path)))
    payload.extend(stream_path)
    payload.extend(b"\0" * (-len(payload) % 4))
    return bytes(payload)


def sprite_link_payload(file_id, path_id):
    payload = bytearray()
    name = b"QA_PRIVATE_SPRITE"
    payload.extend(struct.pack("<i", len(name)) + name)
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(b"\0" * (16 + 8 + 16 + 4 + 8 + 4))
    payload.extend(b"\x01")
    payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(b"\0" * 24)
    tags = (b"QA_PRIVATE_ATLAS",)
    payload.extend(struct.pack("<i", len(tags)))
    for tag in tags:
        payload.extend(struct.pack("<i", len(tag)) + tag)
        payload.extend(b"\0" * (-len(payload) % 4))
    payload.extend(b"\0" * 12)
    payload.extend(struct.pack("<iq", file_id, path_id))
    payload.extend(b"QA_PRIVATE_MESH_PIXEL_SENTINEL")
    return bytes(payload)


def sprite_texture_serialized_file():
    references = (
        (0, 987654321),  # local Texture2D
        (0, 0),  # null
        (1, 987654321),  # external
        (0, 987654322),  # unresolved local reference
    )
    objects = [(213, 1000 + index, 0, sprite_link_payload(*reference))
               for index, reference in enumerate(references)]
    objects.append((28, 987654321, 1, texture2d_payload()))
    metadata = bytearray(b"2019.4.4f1\0")
    metadata.extend(struct.pack("<iB", 19, 0))
    metadata.extend(struct.pack("<i", 2))
    for class_id in (213, 28):
        metadata.extend(struct.pack("<iBh", class_id, 0, -1))
        metadata.extend(b"\xA5" * 16)
    metadata.extend(struct.pack("<i", len(objects)))
    data = bytearray()
    for _, path_id, type_id, payload in objects:
        data.extend(b"\0" * (-len(data) % 4))
        metadata.extend(b"\0" * (-(20 + len(metadata)) % 4))
        metadata.extend(struct.pack("<qIIi", path_id, len(data), len(payload), type_id))
        data.extend(payload)
    metadata.extend(struct.pack("<iii", 0, 0, 0))
    metadata.extend(b"QA_PRIVATE_USER_INFORMATION\0")
    metadata_end = 20 + len(metadata)
    padding = b"\0" * (-metadata_end % 4)
    data_offset = metadata_end + len(padding)
    file_size = data_offset + len(data)
    header = struct.pack(">IIII", len(metadata), file_size, 21, data_offset) + bytes((0, 0, 0, 0))
    return header + metadata + padding + data


def unityfs_bundle(node_bytes, internal_path="private/internal.assets"):
    block_info = bytearray(16)
    block_info.extend(struct.pack(">i", 1))
    block_info.extend(struct.pack(">IIH", len(node_bytes), len(node_bytes), 0))
    block_info.extend(struct.pack(">i", 1))
    block_info.extend(struct.pack(">qqI", 0, len(node_bytes), 0))
    block_info.extend(internal_path.encode("utf-8") + b"\0")

    fields = b"UnityFS\0" + struct.pack(">I", 6) + b"5.x.x\0" + b"2019.4.4f1\0"
    total_size = len(fields) + 20 + len(block_info) + len(node_bytes)
    header = fields + struct.pack(">QIII", total_size, len(block_info), len(block_info), 0)
    return header + bytes(block_info) + node_bytes


class TestUnityFSSerializedFileInspect(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT)
        self.directory = Path(self.temporary_directory.name)
        self.bundle_path = self.directory / "synthetic.bundle"

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_cli(self, bundle_path=None, *arguments):
        return subprocess.run(
            [
                sys.executable,
                str(CLI_PATH),
                "--bundle",
                str(bundle_path or self.bundle_path),
                *arguments,
            ],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_valid_node_prints_only_redacted_json_summary(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21()))

        result = self.run_cli(self.bundle_path, "--node-index", "0")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(
            set(summary),
            {
                "node_index",
                "unity_version",
                "serialized_file_version",
                "target_platform",
                "has_type_tree",
                "counts",
                "class_id_histogram",
            },
        )
        self.assertEqual(summary["node_index"], 0)
        self.assertEqual(summary["unity_version"], "2019.4.4f1")
        self.assertEqual(summary["serialized_file_version"], 21)
        self.assertEqual(summary["target_platform"], 19)
        self.assertEqual(summary["counts"]["objects"], 1)
        self.assertEqual(summary["class_id_histogram"], {"1": 1})
        self.assertNotIn("user_information", summary)
        self.assertEqual(result.stderr, "")
        for private_value in (
            "private/internal.assets",
            "internal.assets",
            "QA_SECRET_INFO",
            "DATA",
            "internal_path",
        ):
            self.assertNotIn(private_value, result.stdout)
            self.assertNotIn(private_value, result.stderr)
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_standalone_no_type_tree_file_prints_only_redacted_metadata(self):
        serialized_path = self.directory / "private-name.assets"
        serialized_path.write_bytes(serialized_file_v21(has_type_tree=False))

        result = subprocess.run(
            [sys.executable, str(CLI_PATH), "--serialized-file", str(serialized_path)],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["serialized_file_version"], 21)
        self.assertFalse(summary["has_type_tree"])
        self.assertEqual(summary["counts"]["objects"], 1)
        self.assertEqual(summary["class_id_histogram"], {"1": 1})
        for private_value in ("private-name.assets", "QA_SECRET_INFO", "DATA", str(serialized_path)):
            self.assertNotIn(private_value, result.stdout)
            self.assertNotIn(private_value, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual([item.name for item in self.directory.iterdir()], ["private-name.assets"])

    def test_texture_summary_is_numeric_and_does_not_print_name_or_pixels(self):
        serialized_path = self.directory / "texture.assets"
        serialized_path.write_bytes(
            serialized_file_v21(
                has_type_tree=False,
                object_class_id=28,
                object_payload=texture2d_payload(),
            )
        )

        result = subprocess.run(
            [sys.executable, str(CLI_PATH), "--serialized-file", str(serialized_path), "--texture-summary"],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(
            summary["texture2d"],
            [{"width": 128, "height": 64, "complete_image_size": 8192, "texture_format": 4}],
        )
        self.assertEqual(summary["class_id_histogram"], {"28": 1})
        for private_value in ("private-texture-x", "PIXEL_SECRET", "path_id"):
            self.assertNotIn(private_value, result.stdout)
            self.assertNotIn(private_value, result.stderr)
        self.assertEqual(result.stderr, "")

    def test_texture_summary_rejects_unsupported_layout_without_partial_json(self):
        serialized_path = self.directory / "texture.assets"
        serialized_path.write_bytes(
            serialized_file_v21(
                unity_version=b"2020.1.0f1",
                has_type_tree=False,
                object_class_id=28,
                object_payload=texture2d_payload(),
            )
        )

        result = subprocess.run(
            [sys.executable, str(CLI_PATH), "--serialized-file", str(serialized_path), "--texture-summary"],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("private-texture-x", result.stderr)
        self.assertNotIn("PIXEL_SECRET", result.stderr)

    def test_texture_summary_option_rejects_bundle_source(self):
        result = self.run_cli(None, "--node-index", "0", "--texture-summary")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")

    def test_sprite_link_summary_emits_only_aggregate_counts(self):
        serialized_path = self.directory / "private-sprite-assets.assets"
        serialized_path.write_bytes(sprite_texture_serialized_file())

        result = subprocess.run(
            [sys.executable, str(CLI_PATH), "--serialized-file", str(serialized_path), "--sprite-link-summary"],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(
            summary["sprite_texture_links"],
            {
                "sprite_count": 4,
                "linked_texture_count": 1,
                "null_reference_count": 1,
                "external_reference_count": 1,
                "unresolved_reference_count": 1,
            },
        )
        for private_value in (
            "QA_PRIVATE_SPRITE",
            "QA_PRIVATE_ATLAS",
            "QA_PRIVATE_MESH_PIXEL_SENTINEL",
            "QA_PRIVATE_USER_INFORMATION",
            "987654321",
            "987654322",
            str(serialized_path),
        ):
            self.assertNotIn(private_value, result.stdout)
            self.assertNotIn(private_value, result.stderr)
        self.assertEqual(result.stderr, "")

    def test_sprite_link_summary_supports_unityfs_node_with_aggregate_counts(self):
        self.bundle_path.write_bytes(unityfs_bundle(sprite_texture_serialized_file()))

        result = self.run_cli(
            self.bundle_path, "--node-index", "0", "--sprite-link-summary"
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(
            summary["sprite_texture_links"],
            {
                "sprite_count": 4,
                "linked_texture_count": 1,
                "null_reference_count": 1,
                "external_reference_count": 1,
                "unresolved_reference_count": 1,
            },
        )
        for private_value in (
            "QA_PRIVATE_SPRITE",
            "QA_PRIVATE_ATLAS",
            "QA_PRIVATE_MESH_PIXEL_SENTINEL",
            "QA_PRIVATE_USER_INFORMATION",
            "987654321",
            "987654322",
            str(self.bundle_path),
        ):
            self.assertNotIn(private_value, result.stdout)
            self.assertNotIn(private_value, result.stderr)
        self.assertEqual(result.stderr, "")

    def test_sprite_link_summary_rejects_incompatible_bundle_summary(self):
        self.bundle_path.write_bytes(unityfs_bundle(sprite_texture_serialized_file()))

        result = self.run_cli(
            self.bundle_path,
            "--node-index",
            "0",
            "--sprite-link-summary",
            "--texture-summary",
        )

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.bundle_path), result.stderr)

    def test_type_tree_disabled_node_prints_only_redacted_json_summary(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21(has_type_tree=False)))

        result = self.run_cli(self.bundle_path, "--node-index", "0")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["serialized_file_version"], 21)
        self.assertEqual(summary["counts"]["objects"], 1)
        self.assertEqual(summary["class_id_histogram"], {"1": 1})
        self.assertNotIn("user_information", summary)
        self.assertNotIn("QA_SECRET_INFO", result.stdout)
        self.assertEqual(result.stderr, "")
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_out_of_range_node_index_fails_without_partial_json_or_export(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21()))

        result = self.run_cli(self.bundle_path, "--node-index", "4")

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.bundle_path), result.stderr)
        self.assertNotIn("private/internal.assets", result.stderr)
        self.assertNotIn("QA_SECRET_INFO", result.stderr)
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_negative_node_index_fails_without_partial_json_or_export(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21()))

        result = self.run_cli(self.bundle_path, "--node-index", "-1")

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.bundle_path), result.stderr)
        self.assertNotIn("private/internal.assets", result.stderr)
        self.assertNotIn("QA_SECRET_INFO", result.stderr)
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_non_v21_node_fails_without_partial_json_or_export(self):
        serialized = bytearray(serialized_file_v21())
        serialized[8:12] = struct.pack(">I", 20)
        self.bundle_path.write_bytes(unityfs_bundle(bytes(serialized)))

        result = self.run_cli(self.bundle_path, "--node-index", "0")

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.bundle_path), result.stderr)
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_unity_version_field_cannot_leak_a_path(self):
        serialized = serialized_file_v21(unity_version=b"C:\\private\\QA_VERSION")
        self.bundle_path.write_bytes(unityfs_bundle(serialized))

        result = self.run_cli(self.bundle_path, "--node-index", "0")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("C:\\private\\QA_VERSION", result.stdout)
        self.assertNotIn("C:\\private\\QA_VERSION", result.stderr)
        self.assertIsNone(json.loads(result.stdout)["unity_version"])

    def test_reparse_point_input_is_rejected_before_reading(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21()))

        with patch.object(inspector, "_has_reparse_point", return_value=True):
            with self.assertRaises(inspector.InspectionError):
                inspector.inspect_node(self.bundle_path, 0)

    def test_node_over_limit_fails_without_json_or_export(self):
        self.bundle_path.write_bytes(unityfs_bundle(serialized_file_v21()))
        stdout = io.StringIO()
        stderr = io.StringIO()

        with patch.object(inspector, "MAX_INSPECTED_NODE_BYTES", 1):
            with redirect_stdout(stdout), redirect_stderr(stderr):
                exit_code = inspector.main(
                    ["--bundle", str(self.bundle_path), "--node-index", "0"]
                )

        self.assertEqual(exit_code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertNotIn(str(self.bundle_path), stderr.getvalue())
        self.assertNotIn("QA_SECRET_INFO", stderr.getvalue())
        self.assertEqual([item.name for item in self.directory.iterdir()], [self.bundle_path.name])

    def test_malformed_bundle_fails_without_json(self):
        self.bundle_path.write_bytes(b"not a UnityFS bundle")

        result = self.run_cli(self.bundle_path, "--node-index", "0")

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.bundle_path), result.stderr)

    def test_scalar_summary_emits_only_selected_texture_integers(self):
        module = inspector.unity_serialized_file
        strings = bytearray()
        offsets = {}

        def offset(value):
            if value not in offsets:
                offsets[value] = len(strings)
                strings.extend(value.encode("utf-8") + b"\0")
            return offsets[value]

        specifications = [
            ("Texture2D", "Base", 0, -1),
            ("SInt32", "m_Width", 1, 4),
            ("SInt32", "m_Height", 1, 4),
            ("SInt32", "m_CompleteImageSize", 1, 4),
            ("SInt32", "m_TextureFormat", 1, 4),
            ("string", "m_Name", 1, -1),
            ("Array", "Array", 2, -1),
            ("int", "size", 3, 4),
            ("char", "data", 3, 1),
            ("TypelessData", "m_ImageData", 1, -1),
            ("int", "size", 2, 4),
            ("UInt8", "data", 2, 1),
        ]
        nodes = tuple(
            module.UnityTypeTreeNode(
                1, level, 0, offset(type_name), offset(name), byte_size, 0, 0, 0
            )
            for type_name, name, level, byte_size in specifications
        )
        type_tree = {"nodes": nodes, "string_buffer": bytes(strings)}
        name = b"private/internal.assets"
        pixels = b"PIXEL_SECRET"
        payload = bytearray(struct.pack("<iiii", 128, 64, 8192, 4))
        payload.extend(struct.pack("<i", len(name)))
        payload.extend(name)
        payload.extend(struct.pack("<i", len(pixels)))
        payload.extend(pixels)

        self.bundle_path.write_bytes(b"\0" * 24 + payload)
        file_size = 24 + len(payload)
        header = {
            "format_version": 21,
            "metadata_size": 4,
            "metadata_offset": 20,
            "metadata_end": 24,
            "data_offset": 24,
            "file_size": file_size,
            "endian": "little",
        }
        serialized_object = module.UnitySerializedObject(7, 0, len(payload), 0, 28)
        serialized_type = module.UnitySerializedType(
            28, False, -1, None, b"\0" * 16, type_tree, ()
        )
        metadata = {
            "header": header,
            "preamble": {
                "unity_version": "2019.4.4f1",
                "target_platform": 19,
                "has_type_tree": True,
            },
            "types": (serialized_type,),
            "objects": (serialized_object,),
            "script_types": (),
            "externals": (),
            "ref_types": (),
        }
        stdout = io.StringIO()
        stderr = io.StringIO()
        with patch.object(module, "parse_serialized_file_metadata_v21", return_value=metadata):
            with redirect_stdout(stdout), redirect_stderr(stderr):
                exit_code = inspector.main(
                    [
                        "--serialized-file",
                        str(self.bundle_path),
                        "--object-index",
                        "0",
                        "--scalar-fields",
                        "width",
                        "height",
                        "complete_image_size",
                        "texture_format",
                    ]
                )

        self.assertEqual(exit_code, 0, stderr.getvalue())
        summary = json.loads(stdout.getvalue())
        self.assertEqual(
            summary["texture2d_scalar_fields"],
            {
                "object_index": 0,
                "fields": {
                    "width": 128,
                    "height": 64,
                    "complete_image_size": 8192,
                    "texture_format": 4,
                },
            },
        )
        for private_value in ("private/internal.assets", "PIXEL_SECRET", "m_Name", "path_id"):
            self.assertNotIn(private_value, stdout.getvalue())


class TestTexture2DPayloadSummary(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT)
        self.path = Path(self.temporary_directory.name) / "texture.assets"

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_cli(self, external_root=None, bc7_mode=False):
        arguments = [
            sys.executable,
            str(CLI_PATH),
            "--serialized-file",
            str(self.path),
            "--texture-payload-summary",
        ]
        if external_root is not None:
            arguments.extend(("--external-data-root", str(external_root)))
        if bc7_mode:
            arguments.append("--bc7-mode-summary")
        return subprocess.run(
            arguments,
            capture_output=True,
            text=True,
            check=False,
        )

    def write_serialized_texture(self, payload, has_type_tree=False):
        self.path.write_bytes(
            serialized_file_v21(
                has_type_tree=has_type_tree,
                object_class_id=28,
                object_payload=payload,
                user_information=b"PRIVATE_TEXTURE_METADATA",
            )
        )

    def test_inline_payload_reports_only_format_and_storage_counts(self):
        pixels = b"PIXEL_SECRET"
        self.write_serialized_texture(
            texture2d_image_payload(3, 2, 2, image_data=pixels)
        )

        result = self.run_cli()

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)["texture2d_payloads"]
        self.assertEqual(summary["texture_count"], 1)
        self.assertEqual(summary["storage_counts"], {"inline": 1, "external": 0, "loaded": 0, "empty": 0})
        self.assertEqual(
            summary["format_counts"],
            {"3": {"inline": 1, "external": 0, "loaded": 0, "empty": 0, "bytes": len(pixels)}},
        )
        for private_value in ("PIXEL_SECRET", "PRIVATE_TEXTURE_METADATA", "private"):
            self.assertNotIn(private_value, result.stdout)

    def test_external_path_is_not_emitted_and_empty_payload_is_reported(self):
        external_path = b"private/resS-section"
        self.write_serialized_texture(
            texture2d_image_payload(
                25,
                4,
                4,
                stream_offset=64,
                stream_size=16,
                stream_path=external_path,
            )
        )

        external_result = self.run_cli()

        self.assertEqual(external_result.returncode, 0, external_result.stderr)
        external = json.loads(external_result.stdout)["texture2d_payloads"]
        self.assertEqual(external["storage_counts"], {"inline": 0, "external": 1, "loaded": 0, "empty": 0})
        self.assertEqual(external["format_counts"]["25"]["bytes"], 16)
        self.assertNotIn(external_path.decode("ascii"), external_result.stdout)
        self.assertNotIn("path_id", external_result.stdout)

        self.write_serialized_texture(texture2d_image_payload(4, 0, 0))
        empty_result = self.run_cli()

        self.assertEqual(empty_result.returncode, 0, empty_result.stderr)
        empty = json.loads(empty_result.stdout)["texture2d_payloads"]
        self.assertEqual(empty["storage_counts"], {"inline": 0, "external": 0, "loaded": 0, "empty": 1})

    def test_payload_summary_rejects_type_tree_input_without_partial_json(self):
        self.write_serialized_texture(
            texture2d_image_payload(1, 2, 2, image_data=b"\0" * 4),
            has_type_tree=True,
        )

        result = self.run_cli()

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(str(self.path), result.stderr)

    def test_external_root_summary_reports_loaded_counts_without_paths(self):
        external_path = b"private/resS-secret.bin"
        external_root = Path(self.temporary_directory.name) / "streams"
        target = external_root / "private" / "resS-secret.bin"
        target.parent.mkdir(parents=True)
        secret_pixels = bytes(range(16))
        target.write_bytes(b"P" * 64 + secret_pixels)
        self.write_serialized_texture(
            texture2d_image_payload(
                25,
                4,
                4,
                stream_offset=64,
                stream_size=len(secret_pixels),
                stream_path=external_path,
            )
        )

        result = self.run_cli(external_root)

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)["texture2d_payloads"]
        self.assertEqual(summary["storage_counts"], {"inline": 0, "external": 0, "loaded": 1, "empty": 0})
        self.assertEqual(
            summary["format_counts"]["25"],
            {"inline": 0, "external": 0, "loaded": 1, "empty": 0, "bytes": 16},
        )
        for private_value in (external_path.decode("ascii"), str(external_root), secret_pixels.hex()):
            self.assertNotIn(private_value, result.stdout)

    def test_external_root_errors_do_not_emit_partial_json_or_path(self):
        external_path = b"../escape.resS"
        external_root = Path(self.temporary_directory.name) / "streams"
        external_root.mkdir()
        self.write_serialized_texture(
            texture2d_image_payload(
                3,
                1,
                1,
                stream_offset=0,
                stream_size=3,
                stream_path=external_path,
            )
        )

        result = self.run_cli(external_root)

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(external_path.decode("ascii"), result.stderr)
        self.assertNotIn(str(external_root), result.stderr)

    def test_external_root_option_requires_payload_summary(self):
        external_root = Path(self.temporary_directory.name) / "streams"
        result = subprocess.run(
            [
                sys.executable,
                str(CLI_PATH),
                "--serialized-file",
                str(self.path),
                "--external-data-root",
                str(external_root),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")

    def test_external_summary_total_byte_budget_is_checked_before_read(self):
        external_root = Path(self.temporary_directory.name) / "budget-streams"
        external_root.mkdir()
        (external_root / "budget.resS").write_bytes(b"123")
        self.write_serialized_texture(
            texture2d_image_payload(
                3,
                1,
                1,
                stream_offset=0,
                stream_size=3,
                stream_path=b"budget.resS",
            )
        )

        external_file = external_root / "budget.resS"
        original_open = Path.open

        def guarded_open(path, *args, **kwargs):
            if path == external_file:
                raise AssertionError("external file opened before budget check")
            return original_open(path, *args, **kwargs)

        with patch.object(inspector, "MAX_TEXTURE2D_STREAM_SUMMARY_BYTES", 2), patch.object(
            inspector.unity_serialized_file.Path, "open", guarded_open
        ):
            with self.assertRaises(
                (inspector.InspectionError, inspector.unity_serialized_file.UnitySerializedFileError)
            ) as caught:
                inspector.inspect_serialized_file(
                    self.path,
                    include_texture_payload_summary=True,
                    external_data_root=external_root,
                )
            self.assertNotIsInstance(caught.exception, AssertionError)

    def test_bc7_mode_summary_counts_all_modes_and_invalid_prefix(self):
        external_root = Path(self.temporary_directory.name) / "bc7-streams"
        external_root.mkdir()
        blocks = b"".join(bytes((1 << mode,)) + bytes(15) for mode in range(8))
        invalid_block = bytes(16)
        payload = blocks + invalid_block
        (external_root / "bc7.resS").write_bytes(payload)
        self.write_serialized_texture(
            texture2d_image_payload(
                25,
                12,
                12,
                stream_offset=0,
                stream_size=len(payload),
                stream_path=b"bc7.resS",
            )
        )

        result = self.run_cli(external_root, bc7_mode=True)

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)["bc7_block_modes"]
        self.assertEqual(summary["texture_count"], 1)
        self.assertEqual(summary["block_count"], 9)
        self.assertEqual(summary["mode_counts"], {str(mode): 1 for mode in range(8)})
        self.assertEqual(summary["invalid_prefix_count"], 1)
        self.assertNotIn(payload.hex(), result.stdout)

    def test_bc7_mode_summary_requires_external_root(self):
        self.write_serialized_texture(
            texture2d_image_payload(
                25,
                4,
                4,
                stream_offset=0,
                stream_size=16,
                stream_path=b"bc7.resS",
            )
        )
        result = self.run_cli(bc7_mode=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")

    def test_bc7_mode_summary_requires_payload_summary(self):
        external_root = Path(self.temporary_directory.name) / "bc7-streams"
        result = subprocess.run(
            [
                sys.executable,
                str(CLI_PATH),
                "--serialized-file",
                str(self.path),
                "--external-data-root",
                str(external_root),
                "--bc7-mode-summary",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")

    def test_bc7_mode_summary_rejects_truncated_stream_without_partial_json(self):
        external_root = Path(self.temporary_directory.name) / "truncated-bc7"
        external_root.mkdir()
        (external_root / "bc7.resS").write_bytes(bytes(15))
        self.write_serialized_texture(
            texture2d_image_payload(
                25,
                4,
                4,
                stream_offset=0,
                stream_size=16,
                stream_path=b"bc7.resS",
            )
        )
        result = self.run_cli(external_root, bc7_mode=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("bc7.resS", result.stderr)

    def test_bc7_mode_summary_ignores_loaded_non_bc7_textures(self):
        external_root = Path(self.temporary_directory.name) / "rgb-streams"
        external_root.mkdir()
        (external_root / "rgb.resS").write_bytes(bytes((1, 2, 3)))
        self.write_serialized_texture(
            texture2d_image_payload(
                3,
                1,
                1,
                stream_offset=0,
                stream_size=3,
                stream_path=b"rgb.resS",
            )
        )
        result = self.run_cli(external_root, bc7_mode=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)["bc7_block_modes"]
        self.assertEqual(summary["texture_count"], 0)
        self.assertEqual(summary["block_count"], 0)
        self.assertEqual(summary["mode_counts"], {str(mode): 0 for mode in range(8)})
        self.assertEqual(summary["invalid_prefix_count"], 0)

    def test_bc7_mode_helper_rejects_partial_blocks(self):
        with self.assertRaisesRegex(inspector.InspectionError, "not aligned"):
            inspector._bc7_block_mode_counts(b"partial")


class TestClassLayoutSummary(unittest.TestCase):
    @staticmethod
    def metadata(
        *,
        class_id=83,
        object_sizes=(180,),
        type_tree_nodes=None,
        has_type_tree=True,
    ):
        if type_tree_nodes is None:
            type_tree_nodes = (
                SimpleNamespace(level=0, byte_size=-1),
                SimpleNamespace(level=1, byte_size=-1),
                SimpleNamespace(level=2, byte_size=4),
                SimpleNamespace(level=2, byte_size=1),
            )
        types = (
            [
                SimpleNamespace(
                    class_id=class_id,
                    type_tree={"nodes": type_tree_nodes},
                )
            ]
            if type_tree_nodes
            else []
        )
        objects = [
            SimpleNamespace(class_id=class_id, byte_size=size)
            for size in object_sizes
        ]
        return {
            "preamble": {"has_type_tree": has_type_tree},
            "types": types,
            "objects": objects,
        }

    def test_summarizes_audio_clip_counts_sizes_and_tree_shape(self):
        summary = inspector._class_layout_summary(
            self.metadata(object_sizes=(180, 8)), 83
        )
        self.assertEqual(
            summary,
            {
                "class_id": 83,
                "object_count": 2,
                "object_bytes_total": 188,
                "object_bytes_max": 180,
                "type_definition_count": 1,
                "type_tree_node_count": 4,
                "type_tree_max_depth": 2,
                "type_tree_variable_size_node_count": 2,
            },
        )

    def test_absent_class_returns_zero_metrics(self):
        summary = inspector._class_layout_summary(
            {"preamble": {"has_type_tree": True}, "types": [], "objects": []},
            83,
        )
        self.assertEqual(summary["object_count"], 0)
        self.assertEqual(summary["object_bytes_total"], 0)
        self.assertEqual(summary["type_tree_node_count"], 0)

    def test_requires_type_tree(self):
        with self.assertRaisesRegex(inspector.InspectionError, "requires TypeTree"):
            inspector._class_layout_summary(
                self.metadata(has_type_tree=False), 83
            )

    def test_rejects_object_byte_total_over_limit(self):
        with self.assertRaisesRegex(inspector.InspectionError, "bytes exceed"):
            inspector._class_layout_summary(
                self.metadata(object_sizes=(inspector.MAX_CLASS_LAYOUT_OBJECT_BYTES + 1,)),
                83,
            )

    def test_rejects_tree_node_limit_before_returning_summary(self):
        nodes = tuple(SimpleNamespace(level=0, byte_size=4) for _ in range(3))
        with patch.object(inspector, "MAX_CLASS_LAYOUT_TYPE_TREE_NODES", 2):
            with self.assertRaisesRegex(inspector.InspectionError, "TypeTree exceeds"):
                inspector._class_layout_summary(
                    self.metadata(type_tree_nodes=nodes), 83
                )

    def test_rejects_object_count_limit(self):
        with patch.object(inspector, "MAX_CLASS_LAYOUT_OBJECTS", 1):
            with self.assertRaisesRegex(inspector.InspectionError, "object count"):
                inspector._class_layout_summary(
                    self.metadata(object_sizes=(4, 8)), 83
                )

    def test_rejects_unsupported_class_id(self):
        with self.assertRaisesRegex(inspector.InspectionError, "class is unsupported"):
            inspector._class_layout_summary(self.metadata(), 75)

    def test_summarizes_deep_animation_clip_tree(self):
        nodes = tuple(
            SimpleNamespace(
                level=index % 10,
                byte_size=-1 if index % 3 == 0 else 4,
            )
            for index in range(524)
        )
        summary = inspector._class_layout_summary(
            self.metadata(class_id=74, object_sizes=(2768,) * 13, type_tree_nodes=nodes),
            74,
        )
        self.assertEqual(summary["object_count"], 13)
        self.assertEqual(summary["object_bytes_total"], 2768 * 13)
        self.assertEqual(summary["type_tree_node_count"], 524)
        self.assertEqual(summary["type_tree_max_depth"], 9)
        self.assertEqual(summary["type_tree_variable_size_node_count"], 175)

    def test_rejects_deep_animation_clip_tree_above_limit(self):
        nodes = tuple(SimpleNamespace(level=index % 4, byte_size=4) for index in range(5))
        with patch.object(inspector, "MAX_CLASS_LAYOUT_TYPE_TREE_NODES", 4):
            with self.assertRaisesRegex(inspector.InspectionError, "TypeTree exceeds"):
                inspector._class_layout_summary(
                    self.metadata(class_id=74, type_tree_nodes=nodes), 74
                )

    def test_rejects_type_tree_depth_above_parser_limit(self):
        nodes = (
            SimpleNamespace(
                level=inspector.unity_serialized_file.MAX_TYPE_TREE_DEPTH + 1,
                byte_size=4,
            ),
        )
        with self.assertRaisesRegex(inspector.InspectionError, "node is invalid"):
            inspector._class_layout_summary(
                self.metadata(class_id=74, type_tree_nodes=nodes), 74
            )

    def test_cli_requires_class_id_and_exclusive_mode(self):
        for argv in (
            ["--serialized-file", "not-opened", "--class-layout-summary"],
            ["--serialized-file", "not-opened", "--class-id", "83"],
            [
                "--serialized-file",
                "not-opened",
                "--class-layout-summary",
                "--class-id",
                "83",
                "--texture-summary",
            ],
        ):
            with self.subTest(argv=argv):
                with self.assertRaises(SystemExit):
                    inspector.main(argv)

    def test_cli_emits_only_class_layout_metrics(self):
        rendered = io.StringIO()
        with patch.object(
            inspector,
            "inspect_serialized_file",
            return_value={
                "class_id_histogram": {"83": 1},
                "private_path": "must-not-be-emitted",
                "class_layout_summary": {
                    "class_id": 83,
                    "object_count": 1,
                    "object_bytes_total": 180,
                },
            },
        ), redirect_stdout(rendered):
            result = inspector.main(
                [
                    "--serialized-file",
                    "unused-but-patched",
                    "--class-layout-summary",
                    "--class-id",
                    "83",
                ]
            )
        self.assertEqual(result, 0)
        parsed = json.loads(rendered.getvalue())
        self.assertEqual(
            parsed,
            {
                "class_layout_summary": {
                    "class_id": 83,
                    "object_count": 1,
                    "object_bytes_total": 180,
                }
            },
        )
        self.assertNotIn("must-not-be-emitted", rendered.getvalue())

    def test_cli_accepts_animation_clip_class_id(self):
        rendered = io.StringIO()
        with patch.object(
            inspector,
            "inspect_serialized_file",
            return_value={"class_layout_summary": {"class_id": 74}},
        ), redirect_stdout(rendered):
            result = inspector.main(
                [
                    "--serialized-file",
                    "unused-but-patched",
                    "--class-layout-summary",
                    "--class-id",
                    "74",
                ]
            )
        self.assertEqual(result, 0)
        self.assertEqual(
            json.loads(rendered.getvalue()),
            {"class_layout_summary": {"class_id": 74}},
        )

    def test_summary_is_deterministic(self):
        metadata = self.metadata(object_sizes=(8, 180))
        first = inspector._class_layout_summary(metadata, 83)
        second = inspector._class_layout_summary(metadata, 83)
        self.assertEqual(first, second)

    def test_class_layout_summary_reads_standalone_metadata_only(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            serialized_path = Path(temporary) / "synthetic.assets"
            serialized_path.write_bytes(serialized_file_v21(object_class_id=83))
            summary = inspector.inspect_serialized_file(
                serialized_path, class_layout_class_id=83
            )
        self.assertEqual(summary["class_layout_summary"]["object_count"], 1)
        self.assertGreater(
            summary["class_layout_summary"]["type_tree_node_count"], 0
        )

    def test_class_layout_summary_reads_selected_unityfs_node_metadata_only(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            bundle_path = Path(temporary) / "synthetic.bundle"
            bundle_path.write_bytes(unityfs_bundle(serialized_file_v21(object_class_id=83)))
            summary = inspector.inspect_node(
                bundle_path, 0, class_layout_class_id=83
            )
        self.assertEqual(summary["class_layout_summary"]["object_count"], 1)
        self.assertGreater(
            summary["class_layout_summary"]["type_tree_node_count"], 0
        )

    def test_cli_rejects_disabled_type_tree_without_partial_json_or_path(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            serialized_path = Path(temporary) / "private-source.assets"
            serialized_path.write_bytes(
                serialized_file_v21(has_type_tree=False, object_class_id=83)
            )
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                result = inspector.main(
                    [
                        "--serialized-file",
                        str(serialized_path),
                        "--class-layout-summary",
                        "--class-id",
                        "83",
                    ]
                )
        self.assertEqual(result, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertNotIn(str(serialized_path), stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
