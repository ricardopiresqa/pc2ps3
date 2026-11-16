#!/usr/bin/env python3
"""Print a bounded JSON summary for one UnityFS SerializedFile v21 node."""

from __future__ import annotations

import argparse
from collections import Counter
import io
import json
import re
import stat
import sys
from pathlib import Path
from typing import Dict, Optional, Tuple, Union

sys.path.insert(0, str(Path(__file__).resolve().parent))

import unity_serialized_file
import unityfs_data
import unityfs_header


class InspectionError(Exception):
    """A safe, user-facing inspection failure without bundle details."""


MAX_INSPECTED_NODE_BYTES = unityfs_data.MAX_NODE_OUTPUT_BYTES
MAX_TEXTURE2D_STREAM_SUMMARY_BYTES = 256 * 1024 * 1024
MAX_CLASS_LAYOUT_OBJECTS = 100_000
MAX_CLASS_LAYOUT_OBJECT_BYTES = 256 * 1024 * 1024
MAX_CLASS_LAYOUT_TYPE_TREE_NODES = 200_000
SUPPORTED_CLASS_LAYOUT_IDS = (74, 83)
_SAFE_TEXTURE_SCALAR_FIELDS = {
    "width": "m_Width",
    "height": "m_Height",
    "complete_image_size": "m_CompleteImageSize",
    "texture_format": "m_TextureFormat",
}


def _has_reparse_point(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) & reparse_flag
    )


def _metadata_summary(metadata: Dict[str, object], node_index=None) -> Dict[str, object]:
    header = metadata["header"]
    preamble = metadata["preamble"]
    raw_unity_version = str(preamble["unity_version"])
    unity_version = (
        raw_unity_version
        if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+-]{0,63}", raw_unity_version)
        else None
    )
    class_id_histogram = Counter(int(item.class_id) for item in metadata["objects"])
    summary = {
        "unity_version": unity_version,
        "serialized_file_version": int(header["format_version"]),
        "target_platform": int(preamble["target_platform"]),
        "has_type_tree": bool(preamble["has_type_tree"]),
        "class_id_histogram": {
            str(class_id): class_id_histogram[class_id]
            for class_id in sorted(class_id_histogram)
        },
        "counts": {
            "types": len(metadata["types"]),
            "objects": len(metadata["objects"]),
            "script_types": len(metadata["script_types"]),
            "external_references": len(metadata["externals"]),
            "reference_types": len(metadata["ref_types"]),
        },
    }
    if node_index is not None:
        summary["node_index"] = int(node_index)
    return summary


def _class_layout_summary(
    metadata: Dict[str, object], class_id: int
) -> Dict[str, int]:
    """Summarize one class from bounded metadata without reading object payloads."""
    if (
        isinstance(class_id, bool)
        or not isinstance(class_id, int)
        or class_id not in SUPPORTED_CLASS_LAYOUT_IDS
    ):
        raise InspectionError("class layout summary class is unsupported")

    preamble = metadata.get("preamble")
    if not isinstance(preamble, dict) or preamble.get("has_type_tree") is not True:
        raise InspectionError("class layout summary requires TypeTree metadata")

    objects = metadata.get("objects")
    types = metadata.get("types")
    if not isinstance(objects, (tuple, list)) or not isinstance(types, (tuple, list)):
        raise InspectionError("class layout metadata is invalid")

    class_objects = []
    for item in objects:
        if item.class_id != class_id:
            continue
        if len(class_objects) >= MAX_CLASS_LAYOUT_OBJECTS:
            raise InspectionError("class layout object count exceeds supported limit")
        class_objects.append(item)

    object_bytes_total = 0
    object_bytes_max = 0
    for item in class_objects:
        size = item.byte_size
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise InspectionError("class layout object size is invalid")
        if size > MAX_CLASS_LAYOUT_OBJECT_BYTES - object_bytes_total:
            raise InspectionError("class layout object bytes exceed supported limit")
        object_bytes_total += size
        object_bytes_max = max(object_bytes_max, size)

    class_types = [item for item in types if item.class_id == class_id]
    tree_node_count = 0
    tree_max_depth = 0
    variable_size_node_count = 0
    for item in class_types:
        tree = item.type_tree
        nodes = tree.get("nodes") if isinstance(tree, dict) else None
        if not isinstance(nodes, (tuple, list)) or not nodes:
            raise InspectionError("class layout TypeTree is missing or invalid")
        if len(nodes) > MAX_CLASS_LAYOUT_TYPE_TREE_NODES - tree_node_count:
            raise InspectionError("class layout TypeTree exceeds supported limit")
        tree_node_count += len(nodes)
        for node in nodes:
            level = node.level
            byte_size = node.byte_size
            if (
                isinstance(level, bool)
                or not isinstance(level, int)
                or level < 0
                or level > unity_serialized_file.MAX_TYPE_TREE_DEPTH
                or isinstance(byte_size, bool)
                or not isinstance(byte_size, int)
                or byte_size < -1
            ):
                raise InspectionError("class layout TypeTree node is invalid")
            tree_max_depth = max(tree_max_depth, level)
            if byte_size == -1:
                variable_size_node_count += 1

    if class_objects and not class_types:
        raise InspectionError("class layout TypeTree definition is missing")

    return {
        "class_id": class_id,
        "object_count": len(class_objects),
        "object_bytes_total": object_bytes_total,
        "object_bytes_max": object_bytes_max,
        "type_definition_count": len(class_types),
        "type_tree_node_count": tree_node_count,
        "type_tree_max_depth": tree_max_depth,
        "type_tree_variable_size_node_count": variable_size_node_count,
    }


def _bc7_block_mode_counts(payload: bytes) -> Tuple[int, Dict[str, int], int]:
    if len(payload) % 16 != 0:
        raise InspectionError("BC7 payload is not aligned to 16-byte blocks")
    mode_counts = {str(mode): 0 for mode in range(8)}
    invalid_prefix_count = 0
    for offset in range(0, len(payload), 16):
        first_byte = payload[offset]
        if first_byte == 0:
            invalid_prefix_count += 1
        else:
            lowest_set_bit = first_byte & -first_byte
            mode_counts[str(lowest_set_bit.bit_length() - 1)] += 1
    return len(payload) // 16, mode_counts, invalid_prefix_count


def _sprite_texture_link_summary(stream, metadata: Dict[str, object]) -> Dict[str, int]:
    preamble = metadata["preamble"]
    if (
        preamble["unity_version"] != "2019.4.4f1"
        or preamble["target_platform"] != 19
    ):
        raise InspectionError("Sprite link summary supports only the validated Unity 2019.4 build")
    objects = metadata["objects"]
    sprites = [item for item in objects if item.class_id == 213]
    if len(sprites) > 4096:
        raise InspectionError("Sprite object count exceeds supported limit")
    path_class_ids = {}
    for item in objects:
        if item.path_id in path_class_ids:
            raise InspectionError("SerializedFile object table has duplicate path IDs")
        path_class_ids[item.path_id] = item.class_id
    local_texture_path_ids = {
        path_id for path_id, class_id in path_class_ids.items() if class_id == 28
    }
    link_counts = Counter()
    for item in sprites:
        status = unity_serialized_file.read_sprite_texture_link_status(
            stream,
            metadata["header"],
            preamble,
            item,
            local_texture_path_ids,
        )
        link_counts[status] += 1
    return {
        "sprite_count": len(sprites),
        "linked_texture_count": link_counts["linked"],
        "null_reference_count": link_counts["null"],
        "external_reference_count": link_counts["external"],
        "unresolved_reference_count": link_counts["unresolved"],
    }


def inspect_serialized_file(
    serialized_file_path: Union[str, Path],
    include_texture_summary: bool = False,
    include_texture_payload_summary: bool = False,
    include_sprite_link_summary: bool = False,
    scalar_object_index: Optional[int] = None,
    scalar_fields: Optional[Tuple[str, ...]] = None,
    external_data_root: Optional[Union[str, Path]] = None,
    include_bc7_mode_summary: bool = False,
    class_layout_class_id: Optional[int] = None,
) -> Dict[str, object]:
    """Read metadata and explicitly selected bounded Texture2D scalar fields."""
    if not isinstance(include_texture_summary, bool):
        raise InspectionError("texture summary option must be boolean")
    if not isinstance(include_texture_payload_summary, bool):
        raise InspectionError("Texture2D payload summary option must be boolean")
    if not isinstance(include_bc7_mode_summary, bool):
        raise InspectionError("BC7 mode summary option must be boolean")
    if external_data_root is not None and not include_texture_payload_summary:
        raise InspectionError("external data root requires Texture2D payload summary")
    if include_bc7_mode_summary and (
        not include_texture_payload_summary or external_data_root is None
    ):
        raise InspectionError("BC7 mode summary requires loaded Texture2D payload summary")
    if not isinstance(include_sprite_link_summary, bool):
        raise InspectionError("Sprite link summary option must be boolean")
    if class_layout_class_id is not None and (
        isinstance(class_layout_class_id, bool)
        or not isinstance(class_layout_class_id, int)
        or class_layout_class_id not in SUPPORTED_CLASS_LAYOUT_IDS
    ):
        raise InspectionError("class layout summary class is unsupported")
    if scalar_fields is None:
        if scalar_object_index is not None:
            raise InspectionError("scalar object index requires selected scalar fields")
    elif (
        include_texture_summary
        or include_texture_payload_summary
        or include_bc7_mode_summary
        or include_sprite_link_summary
        or class_layout_class_id is not None
        or not isinstance(scalar_fields, (tuple, list))
        or not 1 <= len(scalar_fields) <= len(_SAFE_TEXTURE_SCALAR_FIELDS)
        or any(not isinstance(field, str) or field not in _SAFE_TEXTURE_SCALAR_FIELDS for field in scalar_fields)
        or len(set(scalar_fields)) != len(scalar_fields)
        or isinstance(scalar_object_index, bool)
        or not isinstance(scalar_object_index, int)
        or scalar_object_index < 0
    ):
        raise InspectionError("selected scalar summary request is invalid")
    path = Path(serialized_file_path)
    if not path.is_absolute():
        path = Path.cwd() / path
    try:
        if any(_has_reparse_point(candidate) for candidate in (path,) + tuple(path.parents)):
            raise InspectionError("SerializedFile path contains a reparse point")
        if not path.is_file():
            raise InspectionError("SerializedFile is not a readable file")
        file_size = path.stat().st_size
        with path.open("rb") as stream:
            metadata = unity_serialized_file.parse_serialized_file_metadata_v21(
                stream, file_size, allow_type_tree_disabled=True
            )
            texture_summaries = []
            scalar_summary = None
            sprite_link_summary = None
            texture_payload_summary = None
            bc7_mode_summary = None
            if include_texture_summary:
                texture_objects = []
                for item in metadata["objects"]:
                    if item.class_id == 28:
                        if len(texture_objects) >= 4096:
                            raise InspectionError("Texture2D object count exceeds supported limit")
                        texture_objects.append(item)
                for item in texture_objects:
                    texture = unity_serialized_file.read_texture2d_metadata_prefix(
                        stream, metadata["header"], metadata["preamble"], item
                    )
                    texture_summaries.append(
                        {
                            "width": texture.width,
                            "height": texture.height,
                            "complete_image_size": texture.complete_image_size,
                            "texture_format": texture.texture_format,
                        }
                    )
            if include_texture_payload_summary:
                if metadata["preamble"]["has_type_tree"] is not False:
                    raise InspectionError(
                        "Texture2D payload summary supports only the validated no-TypeTree Unity 2019.4 build"
                    )
                storage_counts = {"inline": 0, "external": 0, "loaded": 0, "empty": 0}
                format_counts = {}
                texture_count = 0
                external_bytes_read = 0
                if include_bc7_mode_summary:
                    bc7_mode_summary = {
                        "texture_count": 0,
                        "block_count": 0,
                        "mode_counts": {str(mode): 0 for mode in range(8)},
                        "invalid_prefix_count": 0,
                    }
                for item in metadata["objects"]:
                    if item.class_id != 28:
                        continue
                    if texture_count >= 4096:
                        raise InspectionError("Texture2D object count exceeds supported limit")
                    image = unity_serialized_file.read_texture2d_image_data(
                        stream,
                        metadata["header"],
                        metadata["preamble"],
                        item,
                        external_data_root=external_data_root,
                        max_external_bytes=(
                            min(
                                unity_serialized_file.MAX_TEXTURE2D_IMAGE_BYTES,
                                MAX_TEXTURE2D_STREAM_SUMMARY_BYTES - external_bytes_read,
                            )
                            if external_data_root is not None
                            else unity_serialized_file.MAX_TEXTURE2D_IMAGE_BYTES
                        ),
                    )
                    texture_count += 1
                    storage_counts[image.storage] += 1
                    if image.storage == "loaded" and image.image_data is not None:
                        external_bytes_read += len(image.image_data)
                    if include_bc7_mode_summary and image.texture_format == 25:
                        if image.storage != "loaded" or image.image_data is None:
                            raise InspectionError(
                                "BC7 mode summary requires loaded BC7 payloads"
                            )
                        block_count, mode_counts, invalid_count = _bc7_block_mode_counts(
                            image.image_data
                        )
                        bc7_mode_summary["texture_count"] += 1
                        bc7_mode_summary["block_count"] += block_count
                        bc7_mode_summary["invalid_prefix_count"] += invalid_count
                        for mode, count in mode_counts.items():
                            bc7_mode_summary["mode_counts"][mode] += count
                    counts = format_counts.setdefault(
                        str(image.texture_format),
                        {"inline": 0, "external": 0, "loaded": 0, "empty": 0, "bytes": 0},
                    )
                    counts[image.storage] += 1
                    counts["bytes"] += (
                        len(image.image_data)
                        if image.image_data is not None
                        else image.complete_image_size
                    )
                texture_payload_summary = {
                    "texture_count": texture_count,
                    "storage_counts": storage_counts,
                    "format_counts": format_counts,
                }
            if scalar_fields is not None:
                if not metadata["preamble"]["has_type_tree"]:
                    raise InspectionError("selected scalar fields require a TypeTree-enabled file")
                objects = metadata["objects"]
                if scalar_object_index >= len(objects):
                    raise InspectionError("selected scalar object index is outside the object table")
                item = objects[scalar_object_index]
                if (
                    item.class_id != 28
                    or item.type_id < 0
                    or item.type_id >= len(metadata["types"])
                ):
                    raise InspectionError("selected scalar object is not a supported Texture2D")
                serialized_type = metadata["types"][item.type_id]
                if serialized_type.class_id != 28:
                    raise InspectionError("selected scalar type is not a supported Texture2D")
                payload = unity_serialized_file.read_serialized_object_payload(
                    stream, metadata["header"], item
                )
                requested_names = tuple(_SAFE_TEXTURE_SCALAR_FIELDS[field] for field in scalar_fields)
                decoded = unity_serialized_file.decode_type_tree_object_fields(
                    payload,
                    serialized_type.type_tree,
                    metadata["header"]["endian"],
                    requested_names,
                )
                safe_values = {}
                for field, unity_name in zip(scalar_fields, requested_names):
                    value = decoded[unity_name]
                    if isinstance(value, bool) or not isinstance(value, int):
                        raise InspectionError("selected Texture2D field is not an integer scalar")
                    safe_values[field] = value
                scalar_summary = {
                    "object_index": scalar_object_index,
                    "fields": safe_values,
                }
            if include_sprite_link_summary:
                sprite_link_summary = _sprite_texture_link_summary(stream, metadata)
    except InspectionError:
        raise
    except (OSError, ValueError, KeyError, TypeError, unity_serialized_file.UnitySerializedFileError):
        raise InspectionError("could not inspect SerializedFile metadata")
    summary = _metadata_summary(metadata)
    if include_texture_summary:
        summary["texture2d"] = texture_summaries
    if texture_payload_summary is not None:
        summary["texture2d_payloads"] = texture_payload_summary
    if bc7_mode_summary is not None:
        summary["bc7_block_modes"] = bc7_mode_summary
    if scalar_summary is not None:
        summary["texture2d_scalar_fields"] = scalar_summary
    if sprite_link_summary is not None:
        summary["sprite_texture_links"] = sprite_link_summary
    if class_layout_class_id is not None:
        summary["class_layout_summary"] = _class_layout_summary(
            metadata, class_layout_class_id
        )
    return summary


def inspect_node(
    bundle_path: Union[str, Path],
    node_index: int,
    include_sprite_link_summary: bool = False,
    class_layout_class_id: Optional[int] = None,
) -> Dict[str, object]:
    """Read and summarize one node without exporting its bytes or string fields."""
    if not isinstance(include_sprite_link_summary, bool):
        raise InspectionError("Sprite link summary option must be boolean")
    if class_layout_class_id is not None and (
        isinstance(class_layout_class_id, bool)
        or not isinstance(class_layout_class_id, int)
        or class_layout_class_id not in SUPPORTED_CLASS_LAYOUT_IDS
    ):
        raise InspectionError("class layout summary class is unsupported")
    path = Path(bundle_path)
    if not path.is_absolute():
        if path.drive:
            raise InspectionError("bundle path is not a readable file")
        path = Path.cwd() / path
    try:
        if any(_has_reparse_point(candidate) for candidate in (path,) + tuple(path.parents)):
            raise InspectionError("bundle path contains a reparse point")
        if not path.is_file():
            raise InspectionError("bundle is not a readable file")
        file_size = path.stat().st_size
        with path.open("rb") as stream:
            layout = unityfs_header.parse_data_block_layout(stream, file_size)
            node_bytes = unityfs_data.read_node_bytes(
                stream,
                layout,
                node_index,
                max_output_bytes=MAX_INSPECTED_NODE_BYTES,
            )
    except InspectionError:
        raise
    except (OSError, ValueError, KeyError, TypeError, unityfs_header.UnityFSHeaderError):
        raise InspectionError("could not read the selected UnityFS node") from None

    try:
        node_stream = io.BytesIO(node_bytes)
        metadata = unity_serialized_file.parse_serialized_file_metadata_v21(
            node_stream, len(node_bytes), allow_type_tree_disabled=True
        )
        header = metadata["header"]
        preamble = metadata["preamble"]
    except (
        KeyError,
        TypeError,
        ValueError,
        unity_serialized_file.UnitySerializedFileError,
    ):
        raise InspectionError("selected node is not a valid SerializedFile v21") from None

    raw_unity_version = str(preamble["unity_version"])
    unity_version = (
        raw_unity_version
        if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+-]{0,63}", raw_unity_version)
        else None
    )
    class_id_histogram = Counter(int(item.class_id) for item in metadata["objects"])

    summary = {
        "node_index": node_index,
        "unity_version": unity_version,
        "serialized_file_version": int(header["format_version"]),
        "target_platform": int(preamble["target_platform"]),
        "has_type_tree": bool(preamble["has_type_tree"]),
        "class_id_histogram": {
            str(class_id): class_id_histogram[class_id]
            for class_id in sorted(class_id_histogram)
        },
        "counts": {
            "types": len(metadata["types"]),
            "objects": len(metadata["objects"]),
            "script_types": len(metadata["script_types"]),
            "external_references": len(metadata["externals"]),
            "reference_types": len(metadata["ref_types"]),
        },
    }
    if include_sprite_link_summary:
        summary["sprite_texture_links"] = _sprite_texture_link_summary(
            node_stream, metadata
        )
    if class_layout_class_id is not None:
        summary["class_layout_summary"] = _class_layout_summary(
            metadata, class_layout_class_id
        )
    return summary


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Inspect one UnityFS node or standalone SerializedFile v21 using metadata only."
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--bundle", help="UnityFS bundle to inspect")
    source.add_argument("--serialized-file", help="standalone SerializedFile to inspect")
    parser.add_argument("--node-index", type=int, help="zero-based UnityFS directory index")
    parser.add_argument(
        "--texture-summary",
        action="store_true",
        help="read bounded Texture2D header fields from supported standalone files",
    )
    parser.add_argument(
        "--texture-payload-summary",
        action="store_true",
        help="report aggregate Texture2D payload states without emitting paths or pixels",
    )
    parser.add_argument(
        "--external-data-root",
        type=Path,
        help="local asset root for explicitly loading external Texture2D streams (requires --texture-payload-summary)",
    )
    parser.add_argument(
        "--bc7-mode-summary",
        action="store_true",
        help="aggregate BC7 block modes from loaded streams (requires --texture-payload-summary and --external-data-root)",
    )
    parser.add_argument(
        "--object-index",
        type=int,
        help="zero-based object table index for --scalar-fields",
    )
    parser.add_argument(
        "--scalar-fields",
        nargs="+",
        choices=tuple(_SAFE_TEXTURE_SCALAR_FIELDS),
        help="emit selected safe integer Texture2D fields from a TypeTree",
    )
    parser.add_argument(
        "--class-layout-summary",
        action="store_true",
        help="emit bounded aggregate TypeTree/object-size metrics for one class",
    )
    parser.add_argument(
        "--class-id",
        type=int,
        choices=SUPPORTED_CLASS_LAYOUT_IDS,
        help="class ID selected by --class-layout-summary",
    )
    parser.add_argument(
        "--sprite-link-summary",
        action="store_true",
        help="emit aggregate Sprite-to-local-Texture2D link counts for the validated Unity 2019.4 build",
    )
    return parser


def main(argv=None) -> int:
    parser = _argument_parser()
    args = parser.parse_args(argv)
    if args.external_data_root is not None and not args.texture_payload_summary:
        parser.error("--external-data-root requires --texture-payload-summary")
    if args.bc7_mode_summary and (
        not args.texture_payload_summary or args.external_data_root is None
    ):
        parser.error("--bc7-mode-summary requires --texture-payload-summary and --external-data-root")
    if args.bundle is not None and args.node_index is None:
        parser.error("--node-index is required with --bundle")
    if args.serialized_file is not None and args.node_index is not None:
        parser.error("--node-index cannot be used with --serialized-file")
    if args.bundle is not None and args.texture_summary:
        parser.error("--texture-summary requires --serialized-file")
    if args.bundle is not None and args.texture_payload_summary:
        parser.error("--texture-payload-summary requires --serialized-file")
    if args.bundle is not None and args.scalar_fields is not None:
        parser.error("--scalar-fields requires --serialized-file")
    if args.scalar_fields is None and args.object_index is not None:
        parser.error("--object-index requires --scalar-fields")
    if args.scalar_fields is not None and args.object_index is None:
        parser.error("--scalar-fields requires --object-index")
    if args.scalar_fields is not None and args.texture_summary:
        parser.error("--scalar-fields and --texture-summary are mutually exclusive")
    if args.scalar_fields is not None and args.texture_payload_summary:
        parser.error("--scalar-fields and --texture-payload-summary are mutually exclusive")
    if args.texture_summary and args.texture_payload_summary:
        parser.error("--texture-summary and --texture-payload-summary are mutually exclusive")
    if args.sprite_link_summary and (
        args.scalar_fields is not None or args.texture_summary or args.texture_payload_summary
    ):
        parser.error("--sprite-link-summary is exclusive with other summaries")
    if args.class_layout_summary and args.class_id is None:
        parser.error("--class-layout-summary requires --class-id")
    if args.class_id is not None and not args.class_layout_summary:
        parser.error("--class-id requires --class-layout-summary")
    if args.class_layout_summary and (
        args.texture_summary
        or args.texture_payload_summary
        or args.scalar_fields is not None
        or args.sprite_link_summary
    ):
        parser.error("--class-layout-summary is exclusive with other summaries")
    try:
        if args.bundle is not None:
            summary = inspect_node(
                args.bundle,
                args.node_index,
                include_sprite_link_summary=args.sprite_link_summary,
                class_layout_class_id=(
                    args.class_id if args.class_layout_summary else None
                ),
            )
        else:
            summary = inspect_serialized_file(
                args.serialized_file,
                include_texture_summary=args.texture_summary,
                include_texture_payload_summary=args.texture_payload_summary,
                include_sprite_link_summary=args.sprite_link_summary,
                scalar_object_index=args.object_index,
                scalar_fields=tuple(args.scalar_fields) if args.scalar_fields is not None else None,
                external_data_root=args.external_data_root,
                include_bc7_mode_summary=args.bc7_mode_summary,
                class_layout_class_id=(
                    args.class_id if args.class_layout_summary else None
                ),
            )
    except InspectionError as error:
        sys.stderr.write("{}: error: {}\n".format(parser.prog, error))
        return 2

    if args.class_layout_summary:
        summary = {"class_layout_summary": summary["class_layout_summary"]}
        if args.bundle is not None:
            summary["node_index"] = args.node_index

    sys.stdout.write(json.dumps(summary, sort_keys=True, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
