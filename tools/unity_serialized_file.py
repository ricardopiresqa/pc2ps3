#!/usr/bin/env python3
"""Read bounded Unity SerializedFile metadata and object payloads."""

import math
import struct
from pathlib import Path
from typing import BinaryIO, Dict, NamedTuple, Optional, Tuple, Union


MIN_SUPPORTED_FORMAT_VERSION = 9
MAX_SUPPORTED_FORMAT_VERSION = 21
SERIALIZED_FILE_HEADER_SIZE = 20
MAX_SERIALIZED_FILE_BYTES = 256 * 1024 * 1024
MAX_UNITY_VERSION_STRING_BYTES = 64
MAX_TYPE_TREE_NODES = 100_000
MAX_TYPE_TREE_STRING_BUFFER_BYTES = 1024 * 1024
MAX_TYPE_TREE_STRING_BYTES = 4096
MAX_TYPE_TREE_DEPTH = 64
TYPE_TREE_NODE_SIZE_V19 = 32
MAX_SERIALIZED_TYPES = 10_000
MAX_SERIALIZED_OBJECTS = 250_000
MAX_SERIALIZED_EXTERNALS = 100_000
MAX_SERIALIZED_EXTERNAL_STRING_BYTES = 4096
MAX_SERIALIZED_USER_INFORMATION_BYTES = 1024 * 1024
MAX_TYPE_DEPENDENCIES_PER_TYPE = 100_000
MAX_TOTAL_TYPE_DEPENDENCIES = 250_000
MAX_TOTAL_TYPE_TREE_NODES = 200_000
MAX_TOTAL_TYPE_TREE_STRING_BYTES = 4 * 1024 * 1024
MAX_TEXTURE2D_NAME_BYTES = 4096
MAX_TEXTURE2D_DIMENSION = 65536
MAX_TEXTURE2D_IMAGE_BYTES = 64 * 1024 * 1024
MAX_TEXTURE2D_STREAM_PATH_BYTES = 4096
MAX_SPRITE_OBJECT_PREFIX_BYTES = 256 * 1024
MAX_SPRITE_NAME_BYTES = 4096
MAX_SPRITE_ATLAS_TAGS = 1024
MAX_SPRITE_ATLAS_TAG_BYTES = 64 * 1024


MAX_TYPE_TREE_ARRAY_ELEMENTS = 100_000
MAX_TOTAL_TYPE_TREE_ARRAY_ELEMENTS = MAX_SERIALIZED_FILE_BYTES
MAX_ITERATED_TYPE_TREE_ARRAY_ELEMENTS = 100_000
MAX_TYPE_TREE_STRING_VALUE_BYTES = 1024 * 1024
MAX_TYPE_TREE_SELECTED_FIELDS = 32
TYPE_TREE_ALIGN_BYTES = 1 << 14

_TYPE_TREE_SCALAR_FORMATS = {
    "bool": "?",
    "char": "B",
    "SInt8": "b",
    "UInt8": "B",
    "short": "h",
    "SInt16": "h",
    "unsigned short": "H",
    "UInt16": "H",
    "int": "i",
    "SInt32": "i",
    "Type*": "i",
    "unsigned int": "I",
    "UInt32": "I",
    "long long": "q",
    "SInt64": "q",
    "FileSize": "q",
    "unsigned long long": "Q",
    "UInt64": "Q",
    "float": "f",
    "double": "d",
}
_TYPE_TREE_BYTE_TYPES = frozenset(("char", "SInt8", "UInt8"))


class UnitySerializedFileError(ValueError):
    """Raised for truncated or inconsistent Unity SerializedFile data."""


class UnityTypeTreeNode(NamedTuple):
    version: int
    level: int
    type_flags: int
    type_offset: int
    name_offset: int
    byte_size: int
    index: int
    meta_flags: int
    ref_type_hash: int


class UnitySerializedType(NamedTuple):
    class_id: int
    is_stripped: bool
    script_type_index: int
    script_id: Optional[bytes]
    old_type_hash: bytes
    type_tree: Dict[str, object]
    type_dependencies: Tuple[int, ...]


class UnitySerializedObject(NamedTuple):
    path_id: int
    byte_start: int
    byte_size: int
    type_id: int
    class_id: int


class UnityTexture2DMetadata(NamedTuple):
    width: int
    height: int
    complete_image_size: int
    texture_format: int


class UnitySpriteRegion(NamedTuple):
    x: int
    y: int
    width: int
    height: int


class UnityTexture2DImageData(NamedTuple):
    width: int
    height: int
    complete_image_size: int
    texture_format: int
    mip_count: int
    storage: str
    image_data: Optional[bytes]


class UnitySerializedScriptType(NamedTuple):
    file_index: int
    path_id: int


class UnitySerializedExternalReference(NamedTuple):
    asset_path: str
    guid: bytes
    file_type: int
    path_name: str


class UnitySerializedReferenceType(NamedTuple):
    class_id: int
    is_stripped: bool
    script_type_index: int
    script_id: Optional[bytes]
    old_type_hash: bytes
    type_tree: Dict[str, object]
    class_name: str
    namespace: str
    assembly_name: str


def _read_exact(stream: BinaryIO, size: int) -> bytes:
    output = bytearray()
    while len(output) < size:
        try:
            chunk = stream.read(size - len(output))
        except (OSError, ValueError) as error:
            raise UnitySerializedFileError("cannot read Unity SerializedFile bytes: {}".format(error))
        if not chunk:
            raise UnitySerializedFileError("truncated Unity SerializedFile bytes")
        if len(chunk) > size - len(output):
            raise UnitySerializedFileError("stream returned too many SerializedFile bytes")
        output.extend(chunk)
    return bytes(output)


def parse_serialized_file_header(stream: BinaryIO, available_size: int) -> Dict[str, object]:
    """Validate and describe the fixed 20-byte header for formats 9 through 21."""
    if isinstance(available_size, bool) or not isinstance(available_size, int) or available_size < 0:
        raise UnitySerializedFileError("available SerializedFile size is invalid")
    if available_size < SERIALIZED_FILE_HEADER_SIZE:
        raise UnitySerializedFileError("SerializedFile is smaller than its fixed header")

    try:
        stream.seek(0)
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))

    header = _read_exact(stream, SERIALIZED_FILE_HEADER_SIZE)
    metadata_size, file_size, format_version, data_offset = struct.unpack(">IIII", header[:16])
    endian_marker = header[16]

    if not MIN_SUPPORTED_FORMAT_VERSION <= format_version <= MAX_SUPPORTED_FORMAT_VERSION:
        raise UnitySerializedFileError(
            "unsupported Unity SerializedFile format version: {}".format(format_version)
        )
    if endian_marker not in (0, 1):
        raise UnitySerializedFileError("invalid Unity SerializedFile endian marker")
    if file_size < SERIALIZED_FILE_HEADER_SIZE or file_size > MAX_SERIALIZED_FILE_BYTES:
        raise UnitySerializedFileError("Unity SerializedFile size is outside the supported limit")
    if file_size > available_size:
        raise UnitySerializedFileError("declared Unity SerializedFile size exceeds available bytes")

    metadata_offset = SERIALIZED_FILE_HEADER_SIZE
    metadata_end = metadata_offset + metadata_size
    if metadata_end > file_size or data_offset < metadata_end or data_offset > file_size:
        raise UnitySerializedFileError("Unity SerializedFile metadata/data ranges are inconsistent")

    return {
        "format_version": format_version,
        "file_size": file_size,
        "metadata_size": metadata_size,
        "metadata_offset": metadata_offset,
        "metadata_end": metadata_end,
        "data_offset": data_offset,
        "endian": "big" if endian_marker else "little",
    }


def read_serialized_metadata_preamble(stream: BinaryIO, header: Dict[str, object]) -> Dict[str, object]:
    """Read only the version string, target platform, and TypeTree flag."""
    if not isinstance(header, dict):
        raise UnitySerializedFileError("validated Unity SerializedFile header is required")
    try:
        format_version = header["format_version"]
        file_size = header["file_size"]
        metadata_size = header["metadata_size"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        endian = header["endian"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid Unity SerializedFile header: {}".format(error))
    numeric_fields = (format_version, file_size, metadata_size, metadata_offset, metadata_end, data_offset)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numeric_fields):
        raise UnitySerializedFileError("invalid Unity SerializedFile metadata range")
    if (
        not MIN_SUPPORTED_FORMAT_VERSION <= format_version <= MAX_SUPPORTED_FORMAT_VERSION
        or file_size < SERIALIZED_FILE_HEADER_SIZE
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > file_size
        or endian not in ("little", "big")
    ):
        raise UnitySerializedFileError("invalid Unity SerializedFile metadata range or endian")

    if (
        metadata_size < 0
        or metadata_end != metadata_offset + metadata_size
        or data_offset < metadata_end
        or data_offset > file_size
    ):
        raise UnitySerializedFileError("Unity SerializedFile metadata range is inconsistent")

    try:
        stream.seek(metadata_offset)
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))

    remaining = metadata_end - metadata_offset
    version_bytes = bytearray()
    for _ in range(MAX_UNITY_VERSION_STRING_BYTES + 1):
        if remaining == 0:
            raise UnitySerializedFileError("Unity version string exceeds metadata range")
        char = _read_exact(stream, 1)
        remaining -= 1
        if char == b"\0":
            break
        if len(version_bytes) == MAX_UNITY_VERSION_STRING_BYTES:
            raise UnitySerializedFileError("Unity version string exceeds 64 bytes")
        version_bytes.extend(char)
    else:
        raise UnitySerializedFileError("Unity version string is missing a terminator")
    if not version_bytes:
        raise UnitySerializedFileError("Unity version string is empty")
    try:
        unity_version = bytes(version_bytes).decode("ascii")
    except UnicodeDecodeError:
        raise UnitySerializedFileError("Unity version string is not ASCII")

    if remaining < 5:
        raise UnitySerializedFileError("Unity SerializedFile metadata preamble is truncated")
    platform_bytes = _read_exact(stream, 4)
    type_tree_flag = _read_exact(stream, 1)[0]
    remaining -= 5
    byte_order = "<" if endian == "little" else ">"
    target_platform = struct.unpack(byte_order + "i", platform_bytes)[0]
    if type_tree_flag not in (0, 1):
        raise UnitySerializedFileError("invalid Unity SerializedFile TypeTree flag")

    return {
        "unity_version": unity_version,
        "target_platform": target_platform,
        "has_type_tree": bool(type_tree_flag),
    }


def parse_type_tree_blob(
    stream: BinaryIO,
    metadata_end: int,
    format_version: int,
    endian: str,
) -> Dict[str, object]:
    """Read one bounded TypeTree blob for SerializedFile formats 19 through 21."""
    if (
        isinstance(metadata_end, bool)
        or not isinstance(metadata_end, int)
        or metadata_end < 0
        or metadata_end > MAX_SERIALIZED_FILE_BYTES
    ):
        raise UnitySerializedFileError("Unity SerializedFile metadata end is invalid")
    if isinstance(format_version, bool) or not isinstance(format_version, int) or not 19 <= format_version <= 21:
        raise UnitySerializedFileError("unsupported Unity TypeTree format version")
    if endian not in ("little", "big"):
        raise UnitySerializedFileError("invalid Unity SerializedFile endian")
    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("TypeTree stream must be seekable: {}".format(error))
    if start < 0 or metadata_end - start < 8:
        raise UnitySerializedFileError("Unity TypeTree header exceeds metadata range")

    byte_order = "<" if endian == "little" else ">"
    node_count, string_buffer_size = struct.unpack(byte_order + "ii", _read_exact(stream, 8))
    if node_count <= 0 or node_count > MAX_TYPE_TREE_NODES:
        raise UnitySerializedFileError("Unity TypeTree node count is outside the supported limit")
    if string_buffer_size < 0 or string_buffer_size > MAX_TYPE_TREE_STRING_BUFFER_BYTES:
        raise UnitySerializedFileError("Unity TypeTree string buffer is outside the supported limit")

    node_table_size = node_count * TYPE_TREE_NODE_SIZE_V19
    remaining = metadata_end - start - 8
    if node_table_size > remaining or string_buffer_size > remaining - node_table_size:
        raise UnitySerializedFileError("Unity TypeTree blob exceeds metadata range")

    raw_nodes = _read_exact(stream, node_table_size)
    nodes = []
    previous_level = 0
    node_format = byte_order + "HBBIIiiiQ"
    for index in range(node_count):
        values = struct.unpack_from(node_format, raw_nodes, index * TYPE_TREE_NODE_SIZE_V19)
        node = UnityTypeTreeNode(*values)
        if node.level > MAX_TYPE_TREE_DEPTH:
            raise UnitySerializedFileError("Unity TypeTree depth exceeds the supported limit")
        if index == 0 and node.level != 0:
            raise UnitySerializedFileError("Unity TypeTree root must have level zero")
        if index > 0 and node.level > previous_level + 1:
            raise UnitySerializedFileError("Unity TypeTree node levels are not contiguous")
        for offset in (node.type_offset, node.name_offset):
            if (offset & 0x80000000) == 0 and offset >= string_buffer_size:
                raise UnitySerializedFileError("Unity TypeTree local string offset is outside its buffer")
        nodes.append(node)
        previous_level = node.level

    string_buffer = _read_exact(stream, string_buffer_size)
    return {"nodes": tuple(nodes), "string_buffer": string_buffer}


def resolve_type_tree_string_offset(
    offset: int,
    local_string_buffer: bytes,
    common_string_buffer: Optional[bytes] = None,
) -> str:
    """Resolve one bounded local or common-string TypeTree offset."""
    if isinstance(offset, bool) or not isinstance(offset, int) or not 0 <= offset <= 0xFFFFFFFF:
        raise UnitySerializedFileError("Unity TypeTree string offset is invalid")
    if not isinstance(local_string_buffer, bytes) or len(local_string_buffer) > MAX_TYPE_TREE_STRING_BUFFER_BYTES:
        raise UnitySerializedFileError("Unity TypeTree local string buffer is invalid")

    is_common = bool(offset & 0x80000000)
    string_offset = offset & 0x7FFFFFFF if is_common else offset
    if is_common:
        if (
            not isinstance(common_string_buffer, bytes)
            or len(common_string_buffer) > MAX_TYPE_TREE_STRING_BUFFER_BYTES
        ):
            raise UnitySerializedFileError("Unity TypeTree common string buffer is required and bounded")
        source = common_string_buffer
    else:
        source = local_string_buffer

    if string_offset >= len(source):
        raise UnitySerializedFileError("Unity TypeTree string offset is outside its buffer")
    terminator = source.find(b"\0", string_offset, string_offset + MAX_TYPE_TREE_STRING_BYTES + 1)
    if terminator < 0:
        raise UnitySerializedFileError("Unity TypeTree string is unterminated or exceeds its byte limit")
    try:
        return source[string_offset:terminator].decode("utf-8")
    except UnicodeDecodeError:
        raise UnitySerializedFileError("Unity TypeTree string is not valid UTF-8")


def resolve_type_tree_node_strings(
    node: UnityTypeTreeNode,
    local_string_buffer: bytes,
    common_string_buffer: Optional[bytes] = None,
) -> Dict[str, str]:
    """Resolve a TypeTree node's type and name without changing the node."""
    if not isinstance(node, UnityTypeTreeNode):
        raise UnitySerializedFileError("Unity TypeTree node is invalid")
    return {
        "type_name": resolve_type_tree_string_offset(
            node.type_offset, local_string_buffer, common_string_buffer
        ),
        "name": resolve_type_tree_string_offset(
            node.name_offset, local_string_buffer, common_string_buffer
        ),
    }


def _read_within_metadata(stream: BinaryIO, size: int, metadata_end: int) -> bytes:
    if isinstance(size, bool) or not isinstance(size, int) or size < 0:
        raise UnitySerializedFileError("Unity SerializedFile read size is invalid")
    try:
        position = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if position < 0 or position > metadata_end or size > metadata_end - position:
        raise UnitySerializedFileError("Unity SerializedFile read exceeds metadata range")
    return _read_exact(stream, size)


def _read_bounded_metadata_string(stream: BinaryIO, metadata_end: int, max_bytes: int, field_name: str) -> str:
    if isinstance(max_bytes, bool) or not isinstance(max_bytes, int) or max_bytes <= 0:
        raise UnitySerializedFileError("SerializedFile string limit is invalid")
    output = bytearray()
    for _ in range(max_bytes):
        value = _read_within_metadata(stream, 1, metadata_end)
        if value == b"\0":
            try:
                return output.decode("utf-8")
            except UnicodeDecodeError as error:
                raise UnitySerializedFileError("SerializedFile {} is not valid UTF-8: {}".format(field_name, error))
        output.extend(value)
    raise UnitySerializedFileError("SerializedFile {} exceeds its string limit or lacks a NUL terminator".format(field_name))


def parse_serialized_type_table(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Read a bounded v21 SerializedType table, optionally without TypeTrees."""
    if not isinstance(header, dict) or not isinstance(preamble, dict):
        raise UnitySerializedFileError("validated Unity SerializedFile header and preamble are required")
    try:
        format_version = header["format_version"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        has_type_tree = preamble["has_type_tree"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid Unity SerializedFile metadata: {}".format(error))
    numbers = (format_version, metadata_offset, metadata_end, data_offset, file_size)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
        raise UnitySerializedFileError("invalid Unity SerializedFile metadata range")
    if not isinstance(allow_type_tree_disabled, bool) or (
        format_version != 21
        or (has_type_tree is not True and not (allow_type_tree_disabled and has_type_tree is False))
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
    ):
        raise UnitySerializedFileError("SerializedType parser requires a valid v21 TypeTree metadata range")
    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if start < metadata_offset or metadata_end - start < 4:
        raise UnitySerializedFileError("SerializedType count exceeds metadata range")

    byte_order = "<" if endian == "little" else ">"
    type_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
    if type_count < 0 or type_count > MAX_SERIALIZED_TYPES:
        raise UnitySerializedFileError("SerializedType count is outside the supported limit")

    types = []
    total_dependencies = 0
    total_tree_nodes = 0
    total_string_bytes = 0
    for _ in range(type_count):
        class_id = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
        stripped_value = _read_within_metadata(stream, 1, metadata_end)[0]
        if stripped_value not in (0, 1):
            raise UnitySerializedFileError("invalid SerializedType stripped flag")
        script_type_index = struct.unpack(byte_order + "h", _read_within_metadata(stream, 2, metadata_end))[0]
        script_id = _read_within_metadata(stream, 16, metadata_end) if class_id == 114 else None
        old_type_hash = _read_within_metadata(stream, 16, metadata_end)
        type_tree = (
            parse_type_tree_blob(stream, metadata_end, format_version, endian)
            if has_type_tree
            else {"nodes": (), "string_buffer": b""}
        )
        total_tree_nodes += len(type_tree["nodes"])
        total_string_bytes += len(type_tree["string_buffer"])
        if total_tree_nodes > MAX_TOTAL_TYPE_TREE_NODES:
            raise UnitySerializedFileError("SerializedType table exceeds aggregate TypeTree node limit")
        if total_string_bytes > MAX_TOTAL_TYPE_TREE_STRING_BYTES:
            raise UnitySerializedFileError("SerializedType table exceeds aggregate TypeTree string limit")

        if has_type_tree:
            dependency_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
            if dependency_count < 0 or dependency_count > MAX_TYPE_DEPENDENCIES_PER_TYPE:
                raise UnitySerializedFileError("SerializedType dependency count is outside the supported limit")
            total_dependencies += dependency_count
            if total_dependencies > MAX_TOTAL_TYPE_DEPENDENCIES:
                raise UnitySerializedFileError("SerializedType table exceeds aggregate dependency limit")
            dependency_bytes = _read_within_metadata(stream, dependency_count * 4, metadata_end)
            dependencies = (
                struct.unpack(byte_order + "{}i".format(dependency_count), dependency_bytes)
                if dependency_count
                else ()
            )
        else:
            dependencies = ()
        types.append(
            UnitySerializedType(
                class_id,
                bool(stripped_value),
                script_type_index,
                script_id,
                old_type_hash,
                type_tree,
                dependencies,
            )
        )

    return {"types": tuple(types), "cursor": stream.tell()}


def parse_serialized_object_table(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    serialized_types: Dict[str, object],
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Read bounded v21 object records without consuming script metadata or payloads."""
    if not isinstance(header, dict) or not isinstance(preamble, dict) or not isinstance(serialized_types, dict):
        raise UnitySerializedFileError("validated SerializedFile metadata and type table are required")
    try:
        format_version = header["format_version"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        has_type_tree = preamble["has_type_tree"]
        type_start = serialized_types["cursor"]
        types = serialized_types["types"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid SerializedFile object table metadata: {}".format(error))

    numbers = (format_version, metadata_offset, metadata_end, data_offset, file_size, type_start)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
        raise UnitySerializedFileError("invalid SerializedFile object table range")
    if not isinstance(allow_type_tree_disabled, bool) or (
        format_version != 21
        or (has_type_tree is not True and not (allow_type_tree_disabled and has_type_tree is False))
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
        or not isinstance(types, tuple)
        or any(not isinstance(item, UnitySerializedType) for item in types)
    ):
        raise UnitySerializedFileError("object table parser requires a valid v21 TypeTree metadata range")
    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if start != type_start or start < metadata_offset or metadata_end - start < 4:
        raise UnitySerializedFileError("object table cursor exceeds metadata range or does not follow type table")

    byte_order = "<" if endian == "little" else ">"
    object_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
    if object_count < 0 or object_count > MAX_SERIALIZED_OBJECTS:
        raise UnitySerializedFileError("SerializedFile object count is outside the supported limit")

    objects = []
    for _ in range(object_count):
        padding = (-stream.tell()) % 4
        _read_within_metadata(stream, padding, metadata_end)
        path_id = struct.unpack(byte_order + "q", _read_within_metadata(stream, 8, metadata_end))[0]
        byte_start = struct.unpack(byte_order + "I", _read_within_metadata(stream, 4, metadata_end))[0]
        byte_size = struct.unpack(byte_order + "I", _read_within_metadata(stream, 4, metadata_end))[0]
        type_id = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
        if type_id < 0 or type_id >= len(types):
            raise UnitySerializedFileError("SerializedFile object type ID is outside the type table")
        if data_offset > file_size or byte_start > file_size - data_offset:
            raise UnitySerializedFileError("SerializedFile object data start is outside the file")
        absolute_start = data_offset + byte_start
        if byte_size > file_size - absolute_start:
            raise UnitySerializedFileError("SerializedFile object data range is outside the file")
        objects.append(
            UnitySerializedObject(path_id, byte_start, byte_size, type_id, types[type_id].class_id)
        )

    return {"objects": tuple(objects), "cursor": stream.tell()}


def parse_serialized_script_type_table(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    object_table: Dict[str, object],
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Read bounded v21 script-type identifiers after the object table."""
    if not isinstance(header, dict) or not isinstance(preamble, dict) or not isinstance(object_table, dict):
        raise UnitySerializedFileError("validated SerializedFile metadata and object table are required")
    try:
        format_version = header["format_version"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        has_type_tree = preamble["has_type_tree"]
        table_start = object_table["cursor"]
        objects = object_table["objects"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid SerializedFile script table metadata: {}".format(error))

    numbers = (format_version, metadata_offset, metadata_end, data_offset, file_size, table_start)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
        raise UnitySerializedFileError("invalid SerializedFile script table range")
    if not isinstance(allow_type_tree_disabled, bool) or (
        format_version != 21
        or (has_type_tree is not True and not (allow_type_tree_disabled and has_type_tree is False))
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
        or not isinstance(objects, tuple)
        or any(not isinstance(item, UnitySerializedObject) for item in objects)
    ):
        raise UnitySerializedFileError("script table parser requires a valid v21 TypeTree metadata range")

    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if start != table_start or start < metadata_offset or metadata_end - start < 4:
        raise UnitySerializedFileError("script table cursor does not follow object table or exceeds metadata")

    byte_order = "<" if endian == "little" else ">"
    script_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
    if script_count < 0 or script_count > MAX_SERIALIZED_OBJECTS:
        raise UnitySerializedFileError("SerializedFile script count is outside the supported limit")

    script_types = []
    for _ in range(script_count):
        file_index = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
        padding = (-stream.tell()) % 4
        _read_within_metadata(stream, padding, metadata_end)
        path_id = struct.unpack(byte_order + "q", _read_within_metadata(stream, 8, metadata_end))[0]
        script_types.append(UnitySerializedScriptType(file_index, path_id))

    return {"script_types": tuple(script_types), "cursor": stream.tell()}


def parse_serialized_external_references(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    script_table: Dict[str, object],
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Read bounded v21 external file identifiers after script types."""
    if not isinstance(header, dict) or not isinstance(preamble, dict) or not isinstance(script_table, dict):
        raise UnitySerializedFileError("validated SerializedFile metadata and script table are required")
    try:
        format_version = header["format_version"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        has_type_tree = preamble["has_type_tree"]
        table_start = script_table["cursor"]
        script_types = script_table["script_types"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid SerializedFile external table metadata: {}".format(error))

    numbers = (format_version, metadata_offset, metadata_end, data_offset, file_size, table_start)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
        raise UnitySerializedFileError("invalid SerializedFile external table range")
    if not isinstance(allow_type_tree_disabled, bool) or (
        format_version != 21
        or (has_type_tree is not True and not (allow_type_tree_disabled and has_type_tree is False))
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
        or not isinstance(script_types, tuple)
        or any(not isinstance(item, UnitySerializedScriptType) for item in script_types)
    ):
        raise UnitySerializedFileError("external table parser requires valid v21 script metadata")

    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if start != table_start or start < metadata_offset or metadata_end - start < 4:
        raise UnitySerializedFileError("external table cursor does not follow script table or exceeds metadata")

    byte_order = "<" if endian == "little" else ">"
    external_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
    if external_count < 0 or external_count > MAX_SERIALIZED_EXTERNALS:
        raise UnitySerializedFileError("SerializedFile external count is outside the supported limit")

    external_references = []
    for _ in range(external_count):
        asset_path = _read_bounded_metadata_string(
            stream, metadata_end, MAX_SERIALIZED_EXTERNAL_STRING_BYTES, "external assetPath"
        )
        guid = _read_within_metadata(stream, 16, metadata_end)
        file_type = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
        path_name = _read_bounded_metadata_string(
            stream, metadata_end, MAX_SERIALIZED_EXTERNAL_STRING_BYTES, "external pathName"
        )
        external_references.append(UnitySerializedExternalReference(asset_path, guid, file_type, path_name))

    return {"externals": tuple(external_references), "cursor": stream.tell()}


def parse_serialized_reference_type_table(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    external_table: Dict[str, object],
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Read bounded v21 reference types and trailing user information."""
    if not isinstance(header, dict) or not isinstance(preamble, dict) or not isinstance(external_table, dict):
        raise UnitySerializedFileError("validated SerializedFile metadata and external table are required")
    try:
        format_version = header["format_version"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        has_type_tree = preamble["has_type_tree"]
        table_start = external_table["cursor"]
        external_references = external_table["externals"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid SerializedFile reference type metadata: {}".format(error))

    numbers = (format_version, metadata_offset, metadata_end, data_offset, file_size, table_start)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
        raise UnitySerializedFileError("invalid SerializedFile reference type range")
    if not isinstance(allow_type_tree_disabled, bool) or (
        format_version != 21
        or (has_type_tree is not True and not (allow_type_tree_disabled and has_type_tree is False))
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_end < metadata_offset
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
        or not isinstance(external_references, tuple)
        or any(not isinstance(item, UnitySerializedExternalReference) for item in external_references)
    ):
        raise UnitySerializedFileError("reference type parser requires valid v21 TypeTree metadata")

    try:
        start = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if start != table_start or start < metadata_offset or metadata_end - start < 4:
        raise UnitySerializedFileError("reference type cursor does not follow externals or exceeds metadata")

    byte_order = "<" if endian == "little" else ">"
    reference_type_count = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
    if reference_type_count < 0 or reference_type_count > MAX_SERIALIZED_TYPES:
        raise UnitySerializedFileError("SerializedFile reference type count is outside the supported limit")

    reference_types = []
    total_type_tree_nodes = 0
    total_type_tree_string_bytes = 0
    for _ in range(reference_type_count):
        class_id = struct.unpack(byte_order + "i", _read_within_metadata(stream, 4, metadata_end))[0]
        stripped_value = _read_within_metadata(stream, 1, metadata_end)[0]
        if stripped_value not in (0, 1):
            raise UnitySerializedFileError("invalid SerializedFile reference type stripped flag")
        script_type_index = struct.unpack(byte_order + "h", _read_within_metadata(stream, 2, metadata_end))[0]
        script_id = None
        if script_type_index >= 0 or class_id == 114:
            script_id = _read_within_metadata(stream, 16, metadata_end)
        old_type_hash = _read_within_metadata(stream, 16, metadata_end)
        type_tree = (
            parse_type_tree_blob(stream, metadata_end, format_version, endian)
            if has_type_tree
            else {"nodes": (), "string_buffer": b""}
        )
        nodes = type_tree["nodes"]
        string_buffer = type_tree["string_buffer"]
        if (
            total_type_tree_nodes + len(nodes) > MAX_TOTAL_TYPE_TREE_NODES
            or total_type_tree_string_bytes + len(string_buffer) > MAX_TOTAL_TYPE_TREE_STRING_BYTES
        ):
            raise UnitySerializedFileError("reference type table exceeds aggregate TypeTree limits")
        total_type_tree_nodes += len(nodes)
        total_type_tree_string_bytes += len(string_buffer)
        class_name = _read_bounded_metadata_string(
            stream, metadata_end, MAX_SERIALIZED_EXTERNAL_STRING_BYTES, "reference class name"
        )
        namespace = _read_bounded_metadata_string(
            stream, metadata_end, MAX_SERIALIZED_EXTERNAL_STRING_BYTES, "reference namespace"
        )
        assembly_name = _read_bounded_metadata_string(
            stream, metadata_end, MAX_SERIALIZED_EXTERNAL_STRING_BYTES, "reference assembly name"
        )
        reference_types.append(
            UnitySerializedReferenceType(
                class_id,
                bool(stripped_value),
                script_type_index,
                script_id,
                old_type_hash,
                type_tree,
                class_name,
                namespace,
                assembly_name,
            )
        )

    user_information = _read_bounded_metadata_string(
        stream, metadata_end, MAX_SERIALIZED_USER_INFORMATION_BYTES, "userInformation"
    )
    return {
        "ref_types": tuple(reference_types),
        "user_information": user_information,
        "cursor": stream.tell(),
    }


def parse_serialized_file_metadata_v21(
    stream: BinaryIO,
    available_size: int,
    allow_type_tree_disabled: bool = False,
) -> Dict[str, object]:
    """Parse bounded v21 metadata, optionally accepting files without TypeTrees."""
    if not isinstance(allow_type_tree_disabled, bool):
        raise UnitySerializedFileError("allow_type_tree_disabled must be a boolean")
    header = parse_serialized_file_header(stream, available_size)
    if header["format_version"] != 21:
        raise UnitySerializedFileError("metadata aggregator supports only SerializedFile v21")

    preamble = read_serialized_metadata_preamble(stream, header)
    if preamble["has_type_tree"] is not True and not allow_type_tree_disabled:
        raise UnitySerializedFileError("metadata aggregator requires TypeTree")

    serialized_types = parse_serialized_type_table(
        stream, header, preamble, allow_type_tree_disabled=allow_type_tree_disabled
    )
    objects = parse_serialized_object_table(
        stream, header, preamble, serialized_types, allow_type_tree_disabled=allow_type_tree_disabled
    )
    script_types = parse_serialized_script_type_table(
        stream, header, preamble, objects, allow_type_tree_disabled=allow_type_tree_disabled
    )
    externals = parse_serialized_external_references(
        stream, header, preamble, script_types, allow_type_tree_disabled=allow_type_tree_disabled
    )
    reference_types = parse_serialized_reference_type_table(
        stream, header, preamble, externals, allow_type_tree_disabled=allow_type_tree_disabled
    )

    final_cursor = reference_types["cursor"]
    if final_cursor != header["metadata_end"]:
        raise UnitySerializedFileError("SerializedFile v21 metadata cursor does not reach metadata_end")

    return {
        "header": header,
        "preamble": preamble,
        "types": serialized_types["types"],
        "objects": objects["objects"],
        "script_types": script_types["script_types"],
        "externals": externals["externals"],
        "ref_types": reference_types["ref_types"],
        "user_information": reference_types["user_information"],
        "cursor": final_cursor,
    }


def _validated_serialized_object_payload_range(
    header: Dict[str, object], serialized_object: UnitySerializedObject
) -> Tuple[int, int]:
    if not isinstance(header, dict) or not isinstance(serialized_object, UnitySerializedObject):
        raise UnitySerializedFileError("validated SerializedFile header and object record are required")

    try:
        format_version = header["format_version"]
        metadata_size = header["metadata_size"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid SerializedFile payload metadata: {}".format(error))

    header_numbers = (format_version, metadata_size, metadata_offset, metadata_end, data_offset, file_size)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in header_numbers):
        raise UnitySerializedFileError("invalid SerializedFile payload header values")
    if (
        format_version != 21
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_size < 0
        or metadata_end != metadata_offset + metadata_size
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
    ):
        raise UnitySerializedFileError("object payload requires a valid bounded v21 SerializedFile header")

    object_numbers = (
        serialized_object.path_id,
        serialized_object.byte_start,
        serialized_object.byte_size,
        serialized_object.type_id,
        serialized_object.class_id,
    )
    if any(isinstance(value, bool) or not isinstance(value, int) for value in object_numbers):
        raise UnitySerializedFileError("invalid SerializedFile object record values")
    if (
        serialized_object.path_id < -(1 << 63)
        or serialized_object.path_id >= (1 << 63)
        or serialized_object.byte_start < 0
        or serialized_object.byte_start > 0xFFFFFFFF
        or serialized_object.byte_size < 0
        or serialized_object.byte_size > 0xFFFFFFFF
        or serialized_object.type_id < 0
        or serialized_object.type_id > 0x7FFFFFFF
        or serialized_object.class_id < -(1 << 31)
        or serialized_object.class_id > 0x7FFFFFFF
    ):
        raise UnitySerializedFileError("SerializedFile object record is outside its supported range")

    if serialized_object.byte_start > file_size - data_offset:
        raise UnitySerializedFileError("SerializedFile object payload starts outside the file")
    absolute_start = data_offset + serialized_object.byte_start
    if serialized_object.byte_size > file_size - absolute_start:
        raise UnitySerializedFileError("SerializedFile object payload range is outside the file")
    return absolute_start, serialized_object.byte_size


def read_serialized_object_payload(
    stream: BinaryIO,
    header: Dict[str, object],
    serialized_object: UnitySerializedObject,
) -> bytes:
    """Read exactly one validated v21 object payload into memory."""
    absolute_start, payload_size = _validated_serialized_object_payload_range(
        header, serialized_object
    )

    try:
        stream.seek(absolute_start)
        position = stream.tell()
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
    if position != absolute_start:
        raise UnitySerializedFileError("SerializedFile stream did not seek to the object payload")
    return _read_exact(stream, payload_size)


class _BoundedSerializedObjectReader:
    """Read only requested bytes from one validated object prefix."""

    def __init__(
        self,
        stream: BinaryIO,
        header: Dict[str, object],
        serialized_object: UnitySerializedObject,
        max_prefix_bytes: int,
    ) -> None:
        if (
            isinstance(max_prefix_bytes, bool)
            or not isinstance(max_prefix_bytes, int)
            or max_prefix_bytes < 1
            or max_prefix_bytes > MAX_SERIALIZED_FILE_BYTES
        ):
            raise UnitySerializedFileError("SerializedFile object prefix limit is invalid")
        absolute_start, self.object_size = _validated_serialized_object_payload_range(
            header, serialized_object
        )
        self.stream = stream
        self.max_prefix_bytes = max_prefix_bytes
        self.cursor = 0
        try:
            stream.seek(absolute_start)
            position = stream.tell()
        except (AttributeError, OSError, ValueError) as error:
            raise UnitySerializedFileError("SerializedFile stream must be seekable: {}".format(error))
        if position != absolute_start:
            raise UnitySerializedFileError("SerializedFile stream did not seek to the object prefix")

    def read(self, size: int) -> bytes:
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise UnitySerializedFileError("SerializedFile object prefix read size is invalid")
        end = self.cursor + size
        if end > self.object_size:
            raise UnitySerializedFileError("SerializedFile object prefix exceeds its payload")
        if end > self.max_prefix_bytes:
            raise UnitySerializedFileError("SerializedFile object prefix exceeds its bounded limit")
        data = _read_exact(self.stream, size)
        self.cursor = end
        return data


def decode_type_tree_object_fields(
    payload: bytes,
    type_tree: Dict[str, object],
    endian: str,
    selected_fields: Tuple[str, ...],
    common_string_buffer: Optional[bytes] = None,
) -> Dict[str, object]:
    """Decode selected scalar/string fields from one bounded TypeTree object."""
    if not isinstance(payload, bytes) or len(payload) > MAX_SERIALIZED_FILE_BYTES:
        raise UnitySerializedFileError("TypeTree object payload is invalid or too large")
    if endian not in ("little", "big"):
        raise UnitySerializedFileError("invalid TypeTree object endian")
    if not isinstance(type_tree, dict):
        raise UnitySerializedFileError("TypeTree object definition is invalid")
    nodes = type_tree.get("nodes")
    local_string_buffer = type_tree.get("string_buffer")
    if (
        not isinstance(nodes, (tuple, list))
        or not nodes
        or len(nodes) > MAX_TYPE_TREE_NODES
        or not isinstance(local_string_buffer, bytes)
        or len(local_string_buffer) > MAX_TYPE_TREE_STRING_BUFFER_BYTES
    ):
        raise UnitySerializedFileError("TypeTree object definition is outside supported limits")
    if common_string_buffer is not None and (
        not isinstance(common_string_buffer, bytes)
        or len(common_string_buffer) > MAX_TYPE_TREE_STRING_BUFFER_BYTES
    ):
        raise UnitySerializedFileError("TypeTree common string buffer is invalid or too large")
    if not isinstance(selected_fields, (tuple, list)) or not 1 <= len(selected_fields) <= MAX_TYPE_TREE_SELECTED_FIELDS:
        raise UnitySerializedFileError("TypeTree selected field list is outside supported limits")

    wanted = set()
    for field_name in selected_fields:
        if (
            not isinstance(field_name, str)
            or not field_name
            or len(field_name) > 128
            or not field_name.isascii()
            or not (field_name[0].isalpha() or field_name[0] == "_")
            or any(not (character.isalnum() or character == "_") for character in field_name)
            or field_name in wanted
        ):
            raise UnitySerializedFileError("TypeTree selected field name is invalid")
        wanted.add(field_name)

    children = [[] for _ in nodes]
    resolved = []
    parents = []
    previous_level = -1
    for index, node in enumerate(nodes):
        if not isinstance(node, UnityTypeTreeNode):
            raise UnitySerializedFileError("TypeTree object contains an invalid node")
        numbers = (node.version, node.level, node.type_flags, node.type_offset,
                   node.name_offset, node.byte_size, node.index, node.meta_flags,
                   node.ref_type_hash)
        if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers):
            raise UnitySerializedFileError("TypeTree object node contains invalid numeric fields")
        if node.level < 0 or node.level > MAX_TYPE_TREE_DEPTH or node.meta_flags < 0:
            raise UnitySerializedFileError("TypeTree object node is outside supported limits")
        if index == 0:
            if node.level != 0:
                raise UnitySerializedFileError("TypeTree object root must have level zero")
            parents.append(index)
        else:
            while parents and nodes[parents[-1]].level >= node.level:
                parents.pop()
            if not parents or node.level != nodes[parents[-1]].level + 1:
                raise UnitySerializedFileError("TypeTree object node hierarchy is invalid")
            children[parents[-1]].append(index)
            parents.append(index)
        if node.level > previous_level + 1 and index != 0:
            raise UnitySerializedFileError("TypeTree object node hierarchy skips a level")
        previous_level = node.level
        strings = resolve_type_tree_node_strings(node, local_string_buffer, common_string_buffer)
        type_name = strings["type_name"]
        name = strings["name"]
        if not type_name or len(type_name) > 128 or len(name) > 128:
            raise UnitySerializedFileError("TypeTree object node string is outside supported limits")
        resolved.append((type_name, name))

    byte_order = "<" if endian == "little" else ">"
    cursor = 0
    output = {}
    total_array_elements = 0
    iterated_array_elements = 0

    def read_bytes(size: int) -> bytes:
        nonlocal cursor
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise UnitySerializedFileError("TypeTree value size is invalid")
        end = cursor + size
        if end > len(payload):
            raise UnitySerializedFileError("TypeTree value exceeds object payload")
        value = payload[cursor:end]
        cursor = end
        return value

    def skip_bytes(size: int) -> None:
        nonlocal cursor
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise UnitySerializedFileError("TypeTree skip size is invalid")
        end = cursor + size
        if end > len(payload):
            raise UnitySerializedFileError("TypeTree value exceeds object payload")
        cursor = end

    def align_node(index: int) -> None:
        nonlocal cursor
        if nodes[index].meta_flags & TYPE_TREE_ALIGN_BYTES:
            padding = (-cursor) & 3
            read_bytes(padding)

    def record_selected(index: int, value, scalar: bool) -> None:
        name = resolved[index][1]
        if name not in wanted:
            return
        if not scalar:
            raise UnitySerializedFileError("selected TypeTree field is not a scalar or string")
        if name in output:
            raise UnitySerializedFileError("selected TypeTree field is ambiguous")
        output[name] = value

    def read_scalar(type_name: str):
        format_code = _TYPE_TREE_SCALAR_FORMATS.get(type_name)
        if format_code is None:
            raise UnitySerializedFileError("unsupported TypeTree scalar type")
        size = struct.calcsize(format_code)
        raw = read_bytes(size)
        if type_name == "bool" and raw not in (b"\x00", b"\x01"):
            raise UnitySerializedFileError("TypeTree boolean value is invalid")
        return struct.unpack(byte_order + format_code, raw)[0]

    def array_parts(index: int):
        item_children = children[index]
        if len(item_children) != 2:
            raise UnitySerializedFileError("TypeTree array must contain size and data nodes")
        size_index, data_index = item_children
        if resolved[size_index][1].casefold() != "size" or resolved[data_index][1].casefold() != "data":
            raise UnitySerializedFileError("TypeTree array size/data fields are invalid")
        return size_index, data_index

    def read_string(index: int) -> str:
        item_children = children[index]
        if len(item_children) != 1 or resolved[item_children[0]][0] != "Array":
            raise UnitySerializedFileError("TypeTree string layout is unsupported")
        array_index = item_children[0]
        size_index, data_index = array_parts(array_index)
        count, scalar = read_node(size_index)
        if not scalar or isinstance(count, bool) or not isinstance(count, int):
            raise UnitySerializedFileError("TypeTree string length is invalid")
        if count < 0 or count > MAX_TYPE_TREE_STRING_VALUE_BYTES:
            raise UnitySerializedFileError("TypeTree string length exceeds supported limit")
        data_type, _ = resolved[data_index]
        if (
            data_type not in _TYPE_TREE_BYTE_TYPES
            or children[data_index]
            or nodes[data_index].meta_flags & TYPE_TREE_ALIGN_BYTES
        ):
            raise UnitySerializedFileError("TypeTree string character layout is unsupported")
        raw = read_bytes(count)
        align_node(array_index)
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError:
            raise UnitySerializedFileError("TypeTree string value is not valid UTF-8")

    def read_array(index: int) -> None:
        nonlocal total_array_elements, iterated_array_elements
        size_index, data_index = array_parts(index)
        count, scalar = read_node(size_index)
        if not scalar or isinstance(count, bool) or not isinstance(count, int):
            raise UnitySerializedFileError("TypeTree array length is invalid")
        if count < 0:
            raise UnitySerializedFileError("TypeTree array length exceeds supported limit")
        element_type, _ = resolved[data_index]
        element_children = children[data_index]
        is_byte_array = not element_children and element_type in _TYPE_TREE_BYTE_TYPES
        if not is_byte_array and count > MAX_TYPE_TREE_ARRAY_ELEMENTS:
            raise UnitySerializedFileError("TypeTree array length exceeds supported limit")
        if count > MAX_TOTAL_TYPE_TREE_ARRAY_ELEMENTS - total_array_elements:
            raise UnitySerializedFileError("TypeTree total array elements exceed supported limit")
        total_array_elements += count
        if not element_children and element_type not in _TYPE_TREE_SCALAR_FORMATS and element_type != "string":
            raise UnitySerializedFileError("unsupported TypeTree array element type")
        if count and element_type in _TYPE_TREE_SCALAR_FORMATS and not element_children:
            width = struct.calcsize(_TYPE_TREE_SCALAR_FORMATS[element_type])
            if not (nodes[data_index].meta_flags & TYPE_TREE_ALIGN_BYTES) and element_type != "bool":
                skip_bytes(count * width)
            else:
                if count > MAX_ITERATED_TYPE_TREE_ARRAY_ELEMENTS - iterated_array_elements:
                    raise UnitySerializedFileError("TypeTree array iteration limit exceeded")
                iterated_array_elements += count
                for _ in range(count):
                    read_node(data_index)
        else:
            if count > MAX_ITERATED_TYPE_TREE_ARRAY_ELEMENTS - iterated_array_elements:
                raise UnitySerializedFileError("TypeTree array iteration limit exceeded")
            iterated_array_elements += count
            for _ in range(count):
                read_node(data_index)
        align_node(index)

    def read_node(index: int):
        type_name, name = resolved[index]
        item_children = children[index]
        if type_name in _TYPE_TREE_SCALAR_FORMATS:
            if item_children:
                raise UnitySerializedFileError("TypeTree scalar cannot contain child nodes")
            value = read_scalar(type_name)
            scalar = True
        elif type_name == "string":
            value = read_string(index)
            scalar = True
        elif type_name in ("Array", "TypelessData"):
            value = None
            scalar = False
            read_array(index)
        elif item_children:
            for child_index in item_children:
                read_node(child_index)
            value = None
            scalar = False
        else:
            raise UnitySerializedFileError("unsupported TypeTree value type")
        align_node(index)
        record_selected(index, value, scalar)
        return value, scalar

    read_node(0)
    trailing = len(payload) - cursor
    if trailing > 3 or any(payload[cursor:]):
        raise UnitySerializedFileError("TypeTree did not consume the complete object payload")
    if set(output) != wanted:
        raise UnitySerializedFileError("one or more selected TypeTree fields are missing")
    return output


def _read_sprite_render_data_prefix(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    sprite_object: UnitySerializedObject,
) -> Tuple[Tuple[float, float, float, float], int, int, int, int, int]:
    if (
        not isinstance(preamble, dict)
        or preamble.get("unity_version") != "2019.4.4f1"
        or preamble.get("target_platform") != 19
        or not isinstance(sprite_object, UnitySerializedObject)
        or sprite_object.class_id != 213
    ):
        raise UnitySerializedFileError("Sprite metadata supports only the validated Unity 2019.4 Sprite layout")
    endian = header.get("endian") if isinstance(header, dict) else None
    if endian not in ("little", "big"):
        raise UnitySerializedFileError("Sprite SerializedFile byte order is invalid")

    reader = _BoundedSerializedObjectReader(
        stream,
        header,
        sprite_object,
        MAX_SPRITE_OBJECT_PREFIX_BYTES,
    )
    cursor = 0

    def read_bytes(size: int) -> bytes:
        nonlocal cursor
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise UnitySerializedFileError("Sprite metadata field size is invalid")
        value = reader.read(size)
        cursor += size
        return value

    def skip_bytes(size: int) -> None:
        read_bytes(size)

    def align4() -> None:
        skip_bytes((-cursor) & 3)

    def read_i32() -> int:
        return struct.unpack("<i" if endian == "little" else ">i", read_bytes(4))[0]

    def read_i64() -> int:
        return struct.unpack("<q" if endian == "little" else ">q", read_bytes(8))[0]

    def skip_aligned_string(
        max_bytes: int, remaining_total_bytes: Optional[int] = None
    ) -> int:
        length = read_i32()
        if (
            length < 0
            or length > max_bytes
            or (remaining_total_bytes is not None and length > remaining_total_bytes)
        ):
            raise UnitySerializedFileError("Sprite string length exceeds supported limit")
        raw = read_bytes(length)
        try:
            raw.decode("utf-8")
        except UnicodeDecodeError as error:
            raise UnitySerializedFileError("Sprite metadata string is not valid UTF-8") from error
        align4()
        return length

    # The validated target has no platform-specific Object/EditorExtension prefix.
    skip_aligned_string(MAX_SPRITE_NAME_BYTES)
    rect = struct.unpack("<4f" if endian == "little" else ">4f", read_bytes(16))
    skip_bytes(8)  # m_Offset
    skip_bytes(16)  # m_Border
    skip_bytes(4)  # m_PixelsToUnits
    skip_bytes(8)  # m_Pivot
    skip_bytes(4)  # m_Extrude
    read_bytes(1)  # m_IsPolygon
    align4()
    skip_bytes(24)  # m_RenderDataKey

    tag_count = read_i32()
    if tag_count < 0 or tag_count > MAX_SPRITE_ATLAS_TAGS:
        raise UnitySerializedFileError("Sprite atlas tag count exceeds supported limit")
    total_tag_bytes = 0
    for _ in range(tag_count):
        tag_bytes = skip_aligned_string(
            MAX_SPRITE_NAME_BYTES, MAX_SPRITE_ATLAS_TAG_BYTES - total_tag_bytes
        )
        total_tag_bytes += tag_bytes

    atlas_file_id = read_i32()
    atlas_path_id = read_i64()
    texture_file_id = read_i32()
    texture_path_id = read_i64()
    return rect, tag_count, atlas_file_id, atlas_path_id, texture_file_id, texture_path_id


def _validate_sprite_texture_ids(local_texture_path_ids) -> None:
    if not isinstance(local_texture_path_ids, (set, frozenset)):
        raise UnitySerializedFileError("local Texture2D path ID index is invalid")
    if len(local_texture_path_ids) > MAX_SERIALIZED_OBJECTS or any(
        isinstance(path_id, bool)
        or not isinstance(path_id, int)
        or path_id < -(1 << 63)
        or path_id >= (1 << 63)
        for path_id in local_texture_path_ids
    ):
        raise UnitySerializedFileError("local Texture2D path ID index exceeds supported limits")


def read_sprite_texture_link_status(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    sprite_object: UnitySerializedObject,
    local_texture_path_ids,
) -> str:
    """Read only SpriteRenderData.texture and return an aggregate-safe status."""
    _validate_sprite_texture_ids(local_texture_path_ids)
    _, _, _, _, file_id, path_id = _read_sprite_render_data_prefix(
        stream, header, preamble, sprite_object
    )
    if file_id < 0:
        raise UnitySerializedFileError("Sprite texture reference file ID is invalid")
    if file_id > 0:
        return "external"
    if path_id == 0:
        return "null"
    if path_id in local_texture_path_ids:
        return "linked"
    return "unresolved"


def read_sprite_region(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    sprite_object: UnitySerializedObject,
    local_texture_metadata: Dict[int, UnityTexture2DMetadata],
) -> UnitySpriteRegion:
    """Read an integral direct-texture Sprite region without returning its identity."""
    if not isinstance(local_texture_metadata, dict):
        raise UnitySerializedFileError("local Texture2D metadata index is invalid")
    if len(local_texture_metadata) > MAX_SERIALIZED_OBJECTS or any(
        isinstance(path_id, bool)
        or not isinstance(path_id, int)
        or path_id < -(1 << 63)
        or path_id >= (1 << 63)
        for path_id in local_texture_metadata
    ):
        raise UnitySerializedFileError("local Texture2D metadata index exceeds supported limits")

    rect, tag_count, atlas_file_id, atlas_path_id, file_id, path_id = (
        _read_sprite_render_data_prefix(stream, header, preamble, sprite_object)
    )
    if tag_count or atlas_file_id or atlas_path_id:
        raise UnitySerializedFileError("Sprite atlas layout is unsupported")
    if file_id < 0:
        raise UnitySerializedFileError("Sprite texture reference file ID is invalid")
    if file_id > 0 or path_id == 0:
        raise UnitySerializedFileError("Sprite requires a local Texture2D reference")
    texture = local_texture_metadata.get(path_id)
    if not isinstance(texture, UnityTexture2DMetadata):
        raise UnitySerializedFileError("Sprite Texture2D reference is unresolved")
    if (
        isinstance(texture.width, bool)
        or not isinstance(texture.width, int)
        or not 1 <= texture.width <= MAX_TEXTURE2D_DIMENSION
        or isinstance(texture.height, bool)
        or not isinstance(texture.height, int)
        or not 1 <= texture.height <= MAX_TEXTURE2D_DIMENSION
    ):
        raise UnitySerializedFileError("Sprite Texture2D dimensions are invalid")
    if not all(math.isfinite(value) and value.is_integer() for value in rect):
        raise UnitySerializedFileError("Sprite region coordinates must be finite whole pixels")
    x, y, width, height = (int(value) for value in rect)
    if (
        x < 0
        or y < 0
        or width <= 0
        or height <= 0
        or x + width > texture.width
        or y + height > texture.height
    ):
        raise UnitySerializedFileError("Sprite region is outside the linked Texture2D")
    return UnitySpriteRegion(x, y, width, height)


def read_texture2d_metadata_prefix(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    serialized_object: UnitySerializedObject,
) -> UnityTexture2DMetadata:
    """Read four Texture2D metadata fields without consuming image bytes.

    This layout is intentionally limited to Unity 2019.4.4f1,
    platform 19, SerializedFile v21 objects with TypeTree disabled.
    """
    if (
        not isinstance(header, dict)
        or not isinstance(preamble, dict)
        or not isinstance(serialized_object, UnitySerializedObject)
    ):
        raise UnitySerializedFileError("validated Texture2D metadata is required")
    try:
        format_version = header["format_version"]
        metadata_size = header["metadata_size"]
        metadata_offset = header["metadata_offset"]
        metadata_end = header["metadata_end"]
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
        unity_version = preamble["unity_version"]
        target_platform = preamble["target_platform"]
        has_type_tree = preamble["has_type_tree"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid Texture2D metadata context: {}".format(error))

    header_numbers = (
        format_version,
        metadata_size,
        metadata_offset,
        metadata_end,
        data_offset,
        file_size,
        target_platform,
    )
    if any(isinstance(value, bool) or not isinstance(value, int) for value in header_numbers):
        raise UnitySerializedFileError("invalid Texture2D metadata context values")
    if (
        format_version != 21
        or metadata_offset != SERIALIZED_FILE_HEADER_SIZE
        or metadata_size < 0
        or metadata_end != metadata_offset + metadata_size
        or metadata_end > data_offset
        or data_offset > file_size
        or file_size > MAX_SERIALIZED_FILE_BYTES
        or endian not in ("little", "big")
        or unity_version != "2019.4.4f1"
        or target_platform != 19
        or not isinstance(has_type_tree, bool)
    ):
        raise UnitySerializedFileError("unsupported Texture2D SerializedFile layout")

    object_numbers = (
        serialized_object.path_id,
        serialized_object.byte_start,
        serialized_object.byte_size,
        serialized_object.type_id,
        serialized_object.class_id,
    )
    if any(isinstance(value, bool) or not isinstance(value, int) for value in object_numbers):
        raise UnitySerializedFileError("invalid Texture2D object record")
    if (
        serialized_object.class_id != 28
        or serialized_object.path_id < -(1 << 63)
        or serialized_object.path_id >= (1 << 63)
        or serialized_object.byte_start < 0
        or serialized_object.byte_start > 0xFFFFFFFF
        or serialized_object.byte_size < 0
        or serialized_object.byte_size > 0xFFFFFFFF
        or serialized_object.type_id < 0
        or serialized_object.type_id > 0x7FFFFFFF
        or serialized_object.byte_start > file_size - data_offset
    ):
        raise UnitySerializedFileError("Texture2D object record is outside supported range")

    object_start = data_offset + serialized_object.byte_start
    if serialized_object.byte_size > file_size - object_start:
        raise UnitySerializedFileError("Texture2D object range exceeds SerializedFile")
    object_end = object_start + serialized_object.byte_size

    def read_at(offset: int, size: int) -> bytes:
        if size < 0 or offset < object_start or offset > object_end - size:
            raise UnitySerializedFileError("Texture2D metadata prefix exceeds object range")
        try:
            stream.seek(offset)
            position = stream.tell()
        except (AttributeError, OSError, ValueError) as error:
            raise UnitySerializedFileError("Texture2D stream must be seekable: {}".format(error))
        if position != offset:
            raise UnitySerializedFileError("Texture2D stream did not seek to the metadata field")
        return _read_exact(stream, size)

    byte_order = "<" if endian == "little" else ">"
    name_length = struct.unpack(byte_order + "i", read_at(object_start, 4))[0]
    if name_length < 0 or name_length > MAX_TEXTURE2D_NAME_BYTES:
        raise UnitySerializedFileError("Texture2D name length exceeds supported limit")
    name_end = object_start + 4 + name_length
    aligned_name_end = (name_end + 3) & ~3
    if aligned_name_end > object_end:
        raise UnitySerializedFileError("Texture2D name range exceeds object")

    # Texture base in Unity 2019.4 stores an int32 and a bool, then aligns.
    # Skip the name and fallback-format bytes; neither is returned or decoded.
    fallback_format_offset = aligned_name_end
    fallback_flag_offset = fallback_format_offset + 4
    fallback_flag = read_at(fallback_flag_offset, 1)[0]
    if fallback_flag not in (0, 1):
        raise UnitySerializedFileError("Texture2D fallback flag is invalid")
    fields_offset = (fallback_flag_offset + 1 + 3) & ~3
    width, height, complete_image_size, texture_format = struct.unpack(
        byte_order + "iiii", read_at(fields_offset, 16)
    )
    if (
        width < 0
        or width > MAX_TEXTURE2D_DIMENSION
        or height < 0
        or height > MAX_TEXTURE2D_DIMENSION
        or complete_image_size < 0
        or complete_image_size > MAX_SERIALIZED_FILE_BYTES
        or texture_format < 0
        or texture_format > 255
    ):
        raise UnitySerializedFileError("Texture2D metadata value is outside supported range")
    return UnityTexture2DMetadata(width, height, complete_image_size, texture_format)


def _expected_texture2d_image_size(
    width: int, height: int, texture_format: int, mip_count: int
) -> int:
    if texture_format not in (1, 3, 4, 12, 25):
        raise UnitySerializedFileError("Texture2D format is unsupported")
    if width == 0 or height == 0:
        if width == 0 and height == 0 and mip_count == 1:
            return 0
        raise UnitySerializedFileError("Texture2D dimensions are inconsistent")

    maximum_mips = max(width, height).bit_length()
    if mip_count < 1 or mip_count > min(maximum_mips, 32):
        raise UnitySerializedFileError("Texture2D mip count is outside supported range")

    bytes_per_pixel = {1: 1, 3: 3, 4: 4}.get(texture_format)
    total = 0
    mip_width = width
    mip_height = height
    for _ in range(mip_count):
        if texture_format in (12, 25):
            level_size = ((mip_width + 3) // 4) * ((mip_height + 3) // 4) * 16
        else:
            level_size = mip_width * mip_height * bytes_per_pixel
        if level_size > MAX_TEXTURE2D_IMAGE_BYTES - total:
            raise UnitySerializedFileError("Texture2D image exceeds supported byte limit")
        total += level_size
        mip_width = max(1, mip_width // 2)
        mip_height = max(1, mip_height // 2)
    return total


def _read_texture2d_stream_file(
    external_data_root: Union[str, Path],
    raw_path: bytes,
    stream_offset: int,
    stream_size: int,
    max_bytes: int,
) -> bytes:
    """Read one external Texture2D range while keeping its path private."""
    try:
        relative_text = raw_path.decode("utf-8", errors="strict")
    except UnicodeDecodeError:
        raise UnitySerializedFileError("Texture2D stream path is invalid UTF-8") from None
    normalized = relative_text.replace("\\", "/")
    if normalized.startswith("archive:/"):
        normalized = normalized[len("archive:/") :]
    if (
        not normalized
        or "\x00" in normalized
        or any(ord(character) < 32 for character in normalized)
        or normalized.startswith("/")
        or ":" in normalized
    ):
        raise UnitySerializedFileError("Texture2D stream path is not a safe relative path")
    parts = normalized.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise UnitySerializedFileError("Texture2D stream path is not a safe relative path")
    if (
        isinstance(stream_offset, bool)
        or not isinstance(stream_offset, int)
        or stream_offset < 0
        or isinstance(stream_size, bool)
        or not isinstance(stream_size, int)
        or stream_size <= 0
        or stream_size > MAX_TEXTURE2D_IMAGE_BYTES
        or stream_size > max_bytes
    ):
        raise UnitySerializedFileError("Texture2D stream range exceeds supported read budget")

    try:
        root_text = str(external_data_root)
        if root_text.startswith(("\\\\", "//")):
            raise UnitySerializedFileError("Texture2D stream root must be a local absolute path")
        root_input = Path(external_data_root)
        if not root_input.is_absolute():
            raise UnitySerializedFileError("Texture2D stream root must be absolute")
        root = root_input.resolve(strict=True)
        if not root.is_dir():
            raise UnitySerializedFileError("Texture2D stream root is unavailable")
        target = root
        for part in parts:
            target = target / part
            is_junction = getattr(target, "is_junction", lambda: False)
            if target.is_symlink() or is_junction():
                raise UnitySerializedFileError("Texture2D stream path contains a link")
        target = target.resolve(strict=True)
        if not target.is_relative_to(root) or not target.is_file():
            raise UnitySerializedFileError("Texture2D stream file is outside the asset root")
        file_size = target.stat().st_size
        if stream_offset > file_size or stream_size > file_size - stream_offset:
            raise UnitySerializedFileError("Texture2D stream range exceeds file size")
        with target.open("rb") as payload_stream:
            payload_stream.seek(stream_offset)
            return _read_exact(payload_stream, stream_size)
    except UnitySerializedFileError:
        raise
    except (OSError, RuntimeError, TypeError, ValueError):
        raise UnitySerializedFileError("Texture2D stream file could not be read safely") from None


def read_texture2d_image_data(
    stream: BinaryIO,
    header: Dict[str, object],
    preamble: Dict[str, object],
    serialized_object: UnitySerializedObject,
    *,
    external_data_root: Optional[Union[str, Path]] = None,
    max_external_bytes: int = MAX_TEXTURE2D_IMAGE_BYTES,
) -> UnityTexture2DImageData:
    """Read Texture2D image bytes, optionally resolving a bounded external stream.

    External paths remain internal to this function. Without an explicit root,
    external payloads retain their metadata-only ``external`` result.
    """
    metadata = read_texture2d_metadata_prefix(stream, header, preamble, serialized_object)
    try:
        data_offset = header["data_offset"]
        file_size = header["file_size"]
        endian = header["endian"]
    except (KeyError, TypeError) as error:
        raise UnitySerializedFileError("invalid Texture2D image context: {}".format(error))
    if (
        isinstance(data_offset, bool)
        or not isinstance(data_offset, int)
        or isinstance(file_size, bool)
        or not isinstance(file_size, int)
        or endian not in ("little", "big")
    ):
        raise UnitySerializedFileError("invalid Texture2D image context values")
    if external_data_root is not None and (
        isinstance(max_external_bytes, bool)
        or not isinstance(max_external_bytes, int)
        or max_external_bytes < 0
        or max_external_bytes > MAX_TEXTURE2D_IMAGE_BYTES
    ):
        raise UnitySerializedFileError("Texture2D stream read budget is invalid")

    object_start = data_offset + serialized_object.byte_start
    object_size = serialized_object.byte_size
    if object_start < 0 or object_size < 0 or object_start > file_size - object_size:
        raise UnitySerializedFileError("Texture2D object range exceeds SerializedFile")
    object_end = object_start + object_size
    byte_order = "<" if endian == "little" else ">"

    def read_at(relative_offset: int, size: int) -> bytes:
        if (
            isinstance(relative_offset, bool)
            or not isinstance(relative_offset, int)
            or isinstance(size, bool)
            or not isinstance(size, int)
            or relative_offset < 0
            or size < 0
            or relative_offset > object_size - size
        ):
            raise UnitySerializedFileError("Texture2D field exceeds object range")
        absolute_offset = object_start + relative_offset
        try:
            stream.seek(absolute_offset)
            if stream.tell() != absolute_offset:
                raise UnitySerializedFileError("Texture2D stream did not seek to field")
        except (AttributeError, OSError, ValueError) as error:
            raise UnitySerializedFileError("Texture2D stream must be seekable: {}".format(error))
        return _read_exact(stream, size)

    def align4(value: int) -> int:
        return (value + 3) & ~3

    name_length = struct.unpack(byte_order + "i", read_at(0, 4))[0]
    if name_length < 0 or name_length > MAX_TEXTURE2D_NAME_BYTES:
        raise UnitySerializedFileError("Texture2D name length exceeds supported limit")
    aligned_name_end = align4(4 + name_length)
    fallback_format_offset = aligned_name_end
    fallback_flag_offset = fallback_format_offset + 4
    fallback_flag = read_at(fallback_flag_offset, 1)[0]
    if fallback_flag not in (0, 1):
        raise UnitySerializedFileError("Texture2D fallback flag is invalid")
    fields_offset = align4(fallback_flag_offset + 1)
    values = struct.unpack(byte_order + "iiii", read_at(fields_offset, 16))
    if values != (
        metadata.width,
        metadata.height,
        metadata.complete_image_size,
        metadata.texture_format,
    ):
        raise UnitySerializedFileError("Texture2D metadata changed while reading image")

    fields_end = fields_offset + 16
    mip_count = struct.unpack(byte_order + "i", read_at(fields_end, 4))[0]
    if mip_count < 1 or mip_count > 32:
        raise UnitySerializedFileError("Texture2D mip count is outside supported range")
    expected_size = _expected_texture2d_image_size(
        metadata.width, metadata.height, metadata.texture_format, mip_count
    )
    if (
        metadata.complete_image_size != expected_size
        or metadata.complete_image_size > MAX_TEXTURE2D_IMAGE_BYTES
    ):
        raise UnitySerializedFileError("Texture2D image size does not match its format")

    # The Unity 2019.4.4f1 layout has 48 fixed bytes after m_MipCount before
    # the length-prefixed m_ImageData array. These fields are not needed here.
    image_size_offset = fields_end + 52
    image_data_size = struct.unpack(byte_order + "i", read_at(image_size_offset, 4))[0]
    if image_data_size < 0 or image_data_size > MAX_TEXTURE2D_IMAGE_BYTES:
        raise UnitySerializedFileError("Texture2D image array exceeds supported limit")
    image_data_start = image_size_offset + 4
    image_data_end = image_data_start + image_data_size
    stream_info_offset = align4(image_data_end)

    stream_offset, stream_size = struct.unpack(
        byte_order + "II", read_at(stream_info_offset, 8)
    )
    path_length = struct.unpack(byte_order + "i", read_at(stream_info_offset + 8, 4))[0]
    if path_length < 0 or path_length > MAX_TEXTURE2D_STREAM_PATH_BYTES:
        raise UnitySerializedFileError("Texture2D stream path exceeds supported limit")
    path_start = stream_info_offset + 12
    path_end = path_start + path_length
    object_end_relative = align4(path_end)
    if path_end > object_size or object_end_relative != object_size:
        raise UnitySerializedFileError("Texture2D stream info does not end at object boundary")
    if stream_size > 0xFFFFFFFF - stream_offset:
        raise UnitySerializedFileError("Texture2D stream range overflows")

    if image_data_size == expected_size and image_data_size > 0:
        if stream_offset != 0 or stream_size != 0 or path_length != 0:
            raise UnitySerializedFileError("Texture2D mixes inline and streamed image data")
        storage = "inline"
        image_data = read_at(image_data_start, image_data_size)
    elif image_data_size == 0 and expected_size > 0:
        if stream_size != expected_size or path_length == 0:
            raise UnitySerializedFileError("Texture2D external stream size is invalid")
        if external_data_root is None:
            storage = "external"
            image_data = None
        else:
            path_bytes = read_at(path_start, path_length)
            image_data = _read_texture2d_stream_file(
                external_data_root,
                path_bytes,
                stream_offset,
                stream_size,
                max_external_bytes,
            )
            storage = "loaded"
    elif image_data_size == 0 and expected_size == 0:
        if (
            metadata.width != 0
            or metadata.height != 0
            or stream_offset != 0
            or stream_size != 0
            or path_length != 0
        ):
            raise UnitySerializedFileError("empty Texture2D has inconsistent stream metadata")
        storage = "empty"
        image_data = b""
    else:
        raise UnitySerializedFileError("Texture2D image array size is inconsistent")

    try:
        stream.seek(object_end)
        if stream.tell() != object_end:
            raise UnitySerializedFileError("Texture2D stream did not reach object boundary")
    except (AttributeError, OSError, ValueError) as error:
        raise UnitySerializedFileError("Texture2D stream must be seekable: {}".format(error))
    return UnityTexture2DImageData(
        metadata.width,
        metadata.height,
        metadata.complete_image_size,
        metadata.texture_format,
        mip_count,
        storage,
        image_data,
    )
